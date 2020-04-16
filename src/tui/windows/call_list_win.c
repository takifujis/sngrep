/**************************************************************************
 **
 ** sngrep - SIP Messages flow viewer
 **
 ** Copyright (C) 2013-2019 Ivan Alonso (Kaian)
 ** Copyright (C) 2013-2019 Irontec SL. All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ****************************************************************************/
/**
 * @file call_list_win.c
 * @author Ivan Alonso [aka Kaian] <kaian@irontec.com>
 *
 * @brief Functions to handle Call List window
 *
 */
#include "config.h"
#include <glib.h>
#include <stdio.h>
#include "glib-extra/glib.h"
#include "setting.h"
#include "storage/filter.h"
#ifdef USE_HEP
#include "capture/capture_hep.h"
#endif
#include "tui/tui.h"
#include "tui/dialog.h"
#include "tui/windows/call_list_win.h"
#include "tui/windows/call_flow_win.h"
#include "tui/windows/call_raw_win.h"
#include "tui/windows/save_win.h"
#include "tui/windows/column_select_win.h"

G_DEFINE_TYPE(CallListWindow, call_list_win, TUI_TYPE_WINDOW)

/**
 * @brief Determine if the screen requires redrawn
 *
 * This will query the interface if it requires to be redraw again.
 *
 * @param window UI structure pointer
 * @return true if the panel requires redraw, false otherwise
 */
static gboolean
call_list_win_redraw(G_GNUC_UNUSED Window *window)
{
    return TRUE;
    return storage_calls_changed();
}

/**
 * @brief Resize the windows of Call List
 *
 * This function will be invoked when the ui size has changed
 *
 * @param window UI structure pointer
 * @return 0 if the panel has been resized, -1 otherwise
 */
static int
call_list_win_resize(Window *window)
{
    // Get current screen dimensions
    gint maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);

    // Change the main window size
    wresize(window_get_ncurses_window(window), maxy, maxx);

    // Store new size
    window_set_width(window, maxx);
    window_set_height(window, maxy);
    return 0;
}

/**
 * @brief Draw panel footer
 *
 * This function will draw Call list footer that contains
 * keybinginds
 *
 * @param window UI structure pointer
 */
static gint
call_list_win_draw_footer(Widget *widget)
{
    const char *keybindings[] = {
        key_action_key_str(ACTION_PREV_SCREEN), "Quit",
        key_action_key_str(ACTION_SELECT), "Select",
        key_action_key_str(ACTION_SHOW_HELP), "Help",
        key_action_key_str(ACTION_SAVE), "Save",
        key_action_key_str(ACTION_DISP_FILTER), "Search",
        key_action_key_str(ACTION_SHOW_FLOW_EX), "Extended",
        key_action_key_str(ACTION_CLEAR_CALLS), "Clear",
        key_action_key_str(ACTION_SHOW_FILTERS), "Filter",
        key_action_key_str(ACTION_SHOW_SETTINGS), "Settings",
        key_action_key_str(ACTION_SHOW_COLUMNS), "Columns"
    };

    window_draw_bindings(TUI_WINDOW(widget), keybindings, 20);

    // Chain-up parent draw function
    return TUI_WIDGET_CLASS(call_list_win_parent_class)->draw(widget);
}

static void
call_list_win_handle_action(Widget *sender, KeybindingAction action)
{
    CallListWindow *call_list_win = TUI_CALL_LIST_WIN(widget_get_toplevel(sender));
    CallGroup *group = NULL;
    Call *call = NULL;

    // Check if we handle this action
    switch (action) {
        case ACTION_DISP_FILTER:
            widget_grab_focus(call_list_win->en_dfilter);
            break;
        case ACTION_SHOW_FLOW:
        case ACTION_SHOW_FLOW_EX:
        case ACTION_SHOW_RAW:
            // Create a new group of calls
            group = call_group_clone(table_get_call_group(TUI_TABLE(call_list_win->tb_calls)));

            // If no call is selected, use current call
            if (call_group_count(group) == 0) {
                call = table_get_current_call(TUI_TABLE(call_list_win->tb_calls));
                call_group_add(group, call);
            }

            // Add xcall to the group
            if (action == ACTION_SHOW_FLOW_EX) {
                call = table_get_current_call(TUI_TABLE(call_list_win->tb_calls));
                call_group_add_calls(group, call->xcalls);
                group->callid = call->callid;
            }

            if (action == ACTION_SHOW_RAW) {
                // Create a Call raw panel
                call_raw_win_set_group(tui_create_window(WINDOW_CALL_RAW), group);
            } else {
                // Display current call flow (normal or extended)
                call_flow_win_set_group(tui_create_window(WINDOW_CALL_FLOW), group);
            }
            break;
        case ACTION_SHOW_PROTOCOLS:
            tui_create_window(WINDOW_PROTOCOL_SELECT);
            break;
        case ACTION_SHOW_FILTERS:
            tui_create_window(WINDOW_FILTER);
            break;
        case ACTION_SHOW_COLUMNS:
            tui_create_window(WINDOW_COLUMN_SELECT);
            table_columns_update(TUI_TABLE(call_list_win->tb_calls));
            break;
        case ACTION_SHOW_STATS:
            tui_create_window(WINDOW_STATS);
            break;
        case ACTION_SAVE:
            save_set_group(
                tui_create_window(WINDOW_SAVE),
                table_get_call_group(TUI_TABLE(call_list_win->tb_calls))
            );
            break;
        case ACTION_SHOW_SETTINGS:
            tui_create_window(WINDOW_SETTINGS);
            break;
        case ACTION_TOGGLE_PAUSE:
            // Pause/Resume capture
            capture_manager_get_instance()->paused = !capture_manager_get_instance()->paused;
            break;
        case ACTION_SHOW_HELP:
            window_help(TUI_WINDOW(call_list_win));
            break;
        case ACTION_PREV_SCREEN:
            // Handle quit from this screen unless requested
            if (setting_enabled(SETTING_TUI_EXITPROMPT)) {
                if (dialog_confirm("Confirm exit", "Are you sure you want to quit?", "Yes,No") == 0) {
                    widget_destroy(widget_get_toplevel(sender));
                }
            } else {
                widget_destroy(widget_get_toplevel(sender));
            }
        default:
            break;
    }
}

/**
 * @brief Handle Call list key strokes
 *
 * This function will manage the custom keybindings of the panel.
 * This function return one of the values defined in @key_handler_ret
 *
 * @param window UI structure pointer
 * @param key Pressed keycode
 * @return enum @key_handler_ret
 */
static int
call_list_win_handle_key(Widget *widget, int key)
{
    // Check actions for this key
    KeybindingAction action = ACTION_UNKNOWN;
    while ((action = key_find_action(key, action)) != ACTION_UNKNOWN) {
        // Check if we handle this action
        switch (action) {
            case ACTION_DISP_FILTER:
            case ACTION_SHOW_FLOW:
            case ACTION_SHOW_FLOW_EX:
            case ACTION_SHOW_RAW:
            case ACTION_SHOW_PROTOCOLS:
            case ACTION_SHOW_FILTERS:
            case ACTION_SHOW_COLUMNS:
            case ACTION_SHOW_STATS:
            case ACTION_SAVE:
            case ACTION_SHOW_SETTINGS:
            case ACTION_TOGGLE_PAUSE:
            case ACTION_SHOW_HELP:
            case ACTION_PREV_SCREEN:
                // This panel has handled the key successfully
                call_list_win_handle_action(widget, action);
                break;
            default:
                continue;
        }
    }

    // Return if this panel has handled or not the key
    return (action == ERR) ? KEY_NOT_HANDLED : KEY_HANDLED;
}

/**
 * @brief Request the panel to show its help
 *
 * This function will request to panel to show its help (if any) by
 * invoking its help function.
 *
 * @param window UI structure pointer
 * @return 0 if the screen has help, -1 otherwise
 */
static int
call_list_win_help(G_GNUC_UNUSED Window *window)
{
    WINDOW *help_win;
    int height, width;

    // Create a new panel and show centered
    height = 28;
    width = 65;
    help_win = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);

    // Set the window title
    mvwprintw(help_win, 1, 25, "Call List Help");

    // Write border and boxes around the window
    wattron(help_win, COLOR_PAIR(CP_BLUE_ON_DEF));
    box(help_win, 0, 0);
    mvwhline(help_win, 2, 1, ACS_HLINE, width - 2);
    mvwhline(help_win, 7, 1, ACS_HLINE, width - 2);
    mvwhline(help_win, height - 3, 1, ACS_HLINE, width - 2);
    mvwaddch(help_win, 2, 0, ACS_LTEE);
    mvwaddch(help_win, 7, 0, ACS_LTEE);
    mvwaddch(help_win, height - 3, 0, ACS_LTEE);
    mvwaddch(help_win, 2, 64, ACS_RTEE);
    mvwaddch(help_win, 7, 64, ACS_RTEE);
    mvwaddch(help_win, height - 3, 64, ACS_RTEE);

    // Set the window footer (nice blue?)
    mvwprintw(help_win, height - 2, 20, "Press any key to continue");

    // Some brief explanation about what window shows
    wattron(help_win, COLOR_PAIR(CP_CYAN_ON_DEF));
    mvwprintw(help_win, 3, 2, "This windows show the list of parsed calls from a pcap file ");
    mvwprintw(help_win, 4, 2, "(Offline) or a live capture with libpcap functions (Online).");
    mvwprintw(help_win, 5, 2, "You can configure the columns shown in this screen and some");
    mvwprintw(help_win, 6, 2, "static filters using sngreprc resource file.");
    wattroff(help_win, COLOR_PAIR(CP_CYAN_ON_DEF));

    // A list of available keys in this window
    mvwprintw(help_win, 8, 2, "Available keys:");
    mvwprintw(help_win, 10, 2, "Esc/Q       Exit sngrep.");
    mvwprintw(help_win, 11, 2, "Enter       Show selected calls message flow");
    mvwprintw(help_win, 12, 2, "Space       Select call");
    mvwprintw(help_win, 13, 2, "F1/h        Show this screen");
    mvwprintw(help_win, 14, 2, "F2/S        Save captured packages to a file");
    mvwprintw(help_win, 15, 2, "F3//        Display filtering (match string case insensitive)");
    mvwprintw(help_win, 16, 2, "F4/X        Show selected call-flow (Extended) if available");
    mvwprintw(help_win, 17, 2, "F5/Ctrl-L   Clear call list (can not be undone!)");
    mvwprintw(help_win, 18, 2, "F6/R        Show selected call messages in raw mode");
    mvwprintw(help_win, 19, 2, "F7/F        Show filter options");
    mvwprintw(help_win, 20, 2, "F8/o        Show Settings");
    mvwprintw(help_win, 21, 2, "F10/t       Select displayed columns");
    mvwprintw(help_win, 22, 2, "i/I         Set display filter to invite");
    mvwprintw(help_win, 23, 2, "p           Stop/Resume packet capture");

    // Press any key to close
    wgetch(help_win);
    delwin(help_win);

    return 0;
}


static void
call_list_win_mode_label(Widget *widget)
{
    CaptureManager *capture = capture_manager_get_instance();
    gboolean online = capture_is_online(capture);
    const gchar *device = capture_input_pcap_device(capture);

    g_autoptr(GString) mode = g_string_new("Mode: ");
    g_string_append(mode, online ? "<green>" : "<red>");
    g_string_append(mode, capture_status_desc(capture));

    if (!online) {
        guint progress = capture_manager_load_progress(capture);
        if (progress > 0 && progress < 100) {
            g_string_append_printf(mode, "[%d%%]", progress);
        }
    }

    // Get online mode capture device
    if (device != NULL)
        g_string_append_printf(mode, "[%s]", device);

#ifdef USE_HEP
    const char *eep_port;
    if ((eep_port = capture_output_hep_port(capture))) {
        g_string_append_printf(mode, "[H:%s]", eep_port);
    }
    if ((eep_port = capture_input_hep_port(capture))) {
        g_string_append_printf(mode, "[L:%s]", eep_port);
    }
#endif

    // Set Mode label text
    label_set_text(TUI_LABEL(widget), mode->str);
}

static void
call_list_win_dialog_label(Widget *widget)
{
    g_autoptr(GString) count = g_string_new(NULL);
    // Print Dialogs or Calls in label depending on calls filter
    StorageMatchOpts storageMatchOpts = storage_match_options();
    g_string_append(count, storageMatchOpts.invite ? "Calls: " : "Dialogs: ");
    // Print calls count (also filtered)
    StorageStats stats = storage_calls_stats();
    if (stats.total != stats.displayed) {
        g_string_append_printf(count, "%d / %d", stats.displayed, stats.total);
    } else {
        g_string_append_printf(count, "%d", stats.total);
    }
    // Set Count label text
    label_set_text(TUI_LABEL(widget), count->str);
}

static void
call_list_win_memory_label(Widget *widget)
{
    g_autoptr(GString) memory = g_string_new(NULL);
    if (storage_memory_limit() > 0) {
        g_autofree const gchar *usage = g_format_size_full(
            storage_memory_usage(),
            G_FORMAT_SIZE_IEC_UNITS
        );
        g_autofree const gchar *limit = g_format_size_full(
            storage_memory_limit(),
            G_FORMAT_SIZE_IEC_UNITS
        );
        g_string_append_printf(memory, "Mem: %s / %s", usage, limit);
        label_set_text(TUI_LABEL(widget), memory->str);
    }
}

static void
call_list_win_display_filter(Widget *widget)
{
    const gchar *text = entry_get_text(TUI_ENTRY(widget));
    // Reset filters on each key stroke
    filter_reset_calls();
    filter_set(FILTER_CALL_LIST, strlen(text) ? text : NULL);
}

Window *
call_list_win_new()
{
    Window *window = g_object_new(
        TUI_TYPE_CALL_LIST_WIN,
        "height", getmaxy(stdscr),
        "width", getmaxx(stdscr),
        NULL
    );
    return window;
}

Table *
call_list_win_get_table(Window *window)
{
    CallListWindow *call_list_win = TUI_CALL_LIST_WIN(window);
    return TUI_TABLE(call_list_win->tb_calls);
}

static void
call_list_win_constructed(GObject *object)
{
    CallListWindow *call_list_win = TUI_CALL_LIST_WIN(object);

    // Create menu bar entries
    call_list_win->menu_bar = menu_bar_new();

    // File Menu
    Widget *menu_file = menu_new("File");
    Widget *menu_file_preferences = menu_item_new("Settings");
    menu_item_set_action(TUI_MENU_ITEM(menu_file_preferences), ACTION_SHOW_SETTINGS);
    g_signal_connect(menu_file_preferences, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_SETTINGS));

    Widget *menu_file_save = menu_item_new("Save as ...");
    menu_item_set_action(TUI_MENU_ITEM(menu_file_save), ACTION_SAVE);
    g_signal_connect(menu_file_save, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SAVE));

    Widget *menu_file_exit = menu_item_new("Exit");
    menu_item_set_action(TUI_MENU_ITEM(menu_file_exit), ACTION_PREV_SCREEN);
    g_signal_connect(menu_file_exit, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_PREV_SCREEN));

    // View Menu
    Widget *menu_view = menu_new("View");
    Widget *menu_view_filters = menu_item_new("Filters");
    menu_item_set_action(TUI_MENU_ITEM(menu_view_filters), ACTION_SHOW_FILTERS);
    g_signal_connect(menu_view_filters, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_FILTERS));

    Widget *menu_view_protocols = menu_item_new("Protocols");
    menu_item_set_action(TUI_MENU_ITEM(menu_view_protocols), ACTION_SHOW_PROTOCOLS);
    g_signal_connect(menu_view_protocols, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_PROTOCOLS));

    // Call List menu
    Widget *menu_list = menu_new("Call List");
    Widget *menu_list_columns = menu_item_new("Configure Columns");
    menu_item_set_action(TUI_MENU_ITEM(menu_list_columns), ACTION_SHOW_COLUMNS);
    g_signal_connect(menu_list_columns, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_COLUMNS));

    Widget *menu_list_clear = menu_item_new("Clear List");
    menu_item_set_action(TUI_MENU_ITEM(menu_list_clear), ACTION_CLEAR_CALLS);
    g_signal_connect(menu_list_clear, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_CLEAR_CALLS));

    Widget *menu_list_clear_soft = menu_item_new("Clear filtered calls");
    menu_item_set_action(TUI_MENU_ITEM(menu_list_clear_soft), ACTION_CLEAR_CALLS_SOFT);
    g_signal_connect(menu_list_clear_soft, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_CLEAR_CALLS_SOFT));

    Widget *menu_list_flow = menu_item_new("Show Call Flow");
    menu_item_set_action(TUI_MENU_ITEM(menu_list_flow), ACTION_SHOW_FLOW);
    g_signal_connect(menu_list_flow, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_FLOW));

    Widget *menu_list_flow_ex = menu_item_new("Show Call Flow Extended");
    menu_item_set_action(TUI_MENU_ITEM(menu_list_flow_ex), ACTION_SHOW_FLOW_EX);
    g_signal_connect(menu_list_flow, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_FLOW_EX));

    // Help Menu
    Widget *menu_help = menu_new("Help");
    Widget *menu_help_about = menu_item_new("About");
    menu_item_set_action(TUI_MENU_ITEM(menu_help_about), ACTION_SHOW_HELP);
    g_signal_connect(menu_help_about, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_HELP));


    // Add menubar menus and items
    container_add(TUI_CONTAINER(call_list_win->menu_bar), menu_file);
    container_add(TUI_CONTAINER(menu_file), menu_file_preferences);
    container_add(TUI_CONTAINER(menu_file), menu_file_save);
    container_add(TUI_CONTAINER(menu_file), menu_item_new(NULL));
    container_add(TUI_CONTAINER(menu_file), menu_file_exit);
    container_add(TUI_CONTAINER(call_list_win->menu_bar), menu_view);
    container_add(TUI_CONTAINER(menu_view), menu_view_filters);
    container_add(TUI_CONTAINER(menu_view), menu_view_protocols);
    container_add(TUI_CONTAINER(call_list_win->menu_bar), menu_list);
    container_add(TUI_CONTAINER(menu_list), menu_list_columns);
    container_add(TUI_CONTAINER(menu_list), menu_item_new(NULL));
    container_add(TUI_CONTAINER(menu_list), menu_list_clear);
    container_add(TUI_CONTAINER(menu_list), menu_list_clear_soft);
    container_add(TUI_CONTAINER(menu_list), menu_item_new(NULL));
    container_add(TUI_CONTAINER(menu_list), menu_list_flow);
    container_add(TUI_CONTAINER(menu_list), menu_list_flow_ex);
    container_add(TUI_CONTAINER(call_list_win->menu_bar), menu_help);
    container_add(TUI_CONTAINER(menu_help), menu_help_about);
    container_add(TUI_CONTAINER(call_list_win), call_list_win->menu_bar);

    // First header line
    Widget *header_first = box_new_full(BOX_ORIENTATION_HORIZONTAL, 8, 1);
    widget_set_height(header_first, 1);
    widget_set_vexpand(header_first, FALSE);
    container_add(TUI_CONTAINER(call_list_win), header_first);

    // Mode Label
    Widget *lb_mode = label_new(NULL);
    g_signal_connect(lb_mode, "draw", G_CALLBACK(call_list_win_mode_label), NULL);
    container_add(TUI_CONTAINER(header_first), lb_mode);

    // Dialog Count
    Widget *lb_dialog_cnt = label_new(NULL);
    g_signal_connect(lb_dialog_cnt, "draw", G_CALLBACK(call_list_win_dialog_label), NULL);
    container_add(TUI_CONTAINER(header_first), lb_dialog_cnt);

    // Memory usage
    if (storage_memory_limit() != 0) {
        Widget *lb_memory = label_new(NULL);
        g_signal_connect(lb_memory, "draw", G_CALLBACK(call_list_win_memory_label), NULL);
        container_add(TUI_CONTAINER(header_first), lb_memory);
    }

    // Print Open filename in Offline mode
    CaptureManager *capture = capture_manager_get_instance();
    const gchar *infile = capture_input_pcap_file(capture);
    if (infile != NULL) {
        g_autoptr(GString) file = g_string_new(NULL);
        g_string_append_printf(file, "Filename: %s", infile);
        container_add(TUI_CONTAINER(header_first), label_new(file->str));
    }

    // Show all labels in the line
    container_show_all(TUI_CONTAINER(header_first));

    // Second header line
    Widget *header_second = box_new_full(BOX_ORIENTATION_HORIZONTAL, 5, 1);
    widget_set_vexpand(header_second, FALSE);

    // Show BPF filter if specified in command line
    const gchar *bpf_filter = capture_manager_filter(capture);
    if (bpf_filter != NULL) {
        widget_set_height(header_second, 1);
        g_autoptr(GString) filter = g_string_new("BPF Filter: ");
        g_string_append_printf(filter, "<yellow>%s", bpf_filter);
        container_add(TUI_CONTAINER(header_second), label_new(filter->str));
    }

    // Show Match expression label if specified in command line
    StorageMatchOpts match = storage_match_options();
    if (match.mexpr != NULL) {
        widget_set_height(header_second, 1);
        g_autoptr(GString) match_expression = g_string_new("Match Expression: ");
        g_string_append_printf(match_expression, "<yellow>%s", match.mexpr);
        container_add(TUI_CONTAINER(header_second), label_new(match_expression->str));
    }
    container_add(TUI_CONTAINER(call_list_win), header_second);
    container_show_all(TUI_CONTAINER(header_second));

    // Add Display filter label and entry
    Widget *header_third = box_new_full(BOX_ORIENTATION_HORIZONTAL, 1, 1);
    widget_set_height(header_third, 1);
    widget_set_vexpand(header_third, FALSE);
    Widget *lb_dfilter = label_new("Display Filter:");
    widget_set_hexpand(TUI_WIDGET(lb_dfilter), FALSE);
    container_add(TUI_CONTAINER(header_third), lb_dfilter);
    call_list_win->en_dfilter = entry_new();
    g_signal_connect(call_list_win->en_dfilter, "key-pressed",
                     G_CALLBACK(call_list_win_display_filter), NULL);
    container_add(TUI_CONTAINER(header_third), call_list_win->en_dfilter);
    container_add(TUI_CONTAINER(call_list_win), header_third);
    container_show_all(TUI_CONTAINER(header_third));

    call_list_win->tb_calls = table_new();
//    table_set_rows(TUI_TABLE(call_list), storage_calls());
    table_columns_update(TUI_TABLE(call_list_win->tb_calls));
    g_signal_connect(call_list_win->tb_calls, "activate",
                     G_CALLBACK(call_list_win_handle_action),
                     GINT_TO_POINTER(ACTION_SHOW_FLOW));

    container_add(TUI_CONTAINER(call_list_win), call_list_win->tb_calls);
    widget_show(TUI_WIDGET(call_list_win->tb_calls));

    // Start with the call list focused
    window_set_default_focus(TUI_WINDOW(call_list_win), call_list_win->tb_calls);

    // Chain-up parent constructed
    G_OBJECT_CLASS(call_list_win_parent_class)->constructed(object);
}

static void
call_list_win_class_init(CallListWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->constructed = call_list_win_constructed;

    WindowClass *window_class = TUI_WINDOW_CLASS(klass);
    window_class->redraw = call_list_win_redraw;
    window_class->resize = call_list_win_resize;
    window_class->help = call_list_win_help;

    WidgetClass *widget_class = TUI_WIDGET_CLASS(klass);
    widget_class->draw = call_list_win_draw_footer;
    widget_class->key_pressed = call_list_win_handle_key;
}

static void
call_list_win_init(CallListWindow *self)
{
    // Set parent attributes
    window_set_window_type(TUI_WINDOW(self), WINDOW_CALL_LIST);
}
