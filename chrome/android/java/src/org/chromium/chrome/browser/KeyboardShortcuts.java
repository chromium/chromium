// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.task_manager.TaskManager;
import org.chromium.chrome.browser.task_manager.TaskManagerFactory;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/** Implements app-level keyboard shortcuts for ChromeTabbedActivity and DocumentActivity. */
public class KeyboardShortcuts {

    private static final int CTRL = 1 << 31;
    private static final int ALT = 1 << 30;
    private static final int SHIFT = 1 << 29;

    private KeyboardShortcuts() {}

    // KeyboardShortcutsSemanticMeaning defined in
    // tools/metrics/histograms/metadata/accessibility/enums.xml.
    // Changing this also requires creating a new histogram: See
    // https://chromium.googlesource.com/chromium/src/+/HEAD/tools/metrics/histograms/README.md#revising.
    // LINT.IfChange(KeyboardShortcutsSemanticMeaning)
    @IntDef({
        KeyboardShortcutsSemanticMeaning.UNKNOWN,
        KeyboardShortcutsSemanticMeaning.OPEN_RECENTLY_CLOSED_TAB,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB_INCOGNITO,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_WINDOW,
        KeyboardShortcutsSemanticMeaning.RELOAD_TAB,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_RELOAD_TAB_BYPASSING_CACHE,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TAB_SEARCH,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_MULTITASK_MENU,
        KeyboardShortcutsSemanticMeaning.CLOSE_TAB,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CLOSE_WINDOW,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_QUIT_CHROME,
        KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX,
        KeyboardShortcutsSemanticMeaning.JUMP_TO_SEARCH,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_DOWN,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_UP,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_TOOLBAR,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_BOOKMARKS,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_CARET_BROWSING,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_ON_INACTIVE_DIALOGS,
        KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS,
        KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BOOKMARK_ALL_TABS,
        KeyboardShortcutsSemanticMeaning.TOGGLE_BOOKMARK_BAR,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_IMMERSIVE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_EXIT_IMMERSIVE,
        KeyboardShortcutsSemanticMeaning.DEV_TOOLS,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_CONSOLE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_INSPECT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_TOGGLE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_VIEW_SOURCE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TASK_MANAGER,
        KeyboardShortcutsSemanticMeaning.SAVE_PAGE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SHOW_DOWNLOADS,
        KeyboardShortcutsSemanticMeaning.OPEN_HISTORY,
        KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK,
        KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CLEAR_BROWSING_DATA,
        KeyboardShortcutsSemanticMeaning.PRINT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BASIC_PRINT,
        KeyboardShortcutsSemanticMeaning.ZOOM_IN,
        KeyboardShortcutsSemanticMeaning.ZOOM_OUT,
        KeyboardShortcutsSemanticMeaning.ZOOM_RESET,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_AVATAR_MENU,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FEEDBACK_FORM,
        KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_HOME,
        KeyboardShortcutsSemanticMeaning.OPEN_HELP,
        KeyboardShortcutsSemanticMeaning.OPEN_MENU,
        KeyboardShortcutsSemanticMeaning.CUSTOM_EXTENSION_SHORTCUT,
        KeyboardShortcutsSemanticMeaning.MAX_VALUE
    })
    public @interface KeyboardShortcutsSemanticMeaning {
        // TODO(crbug.com/402775002): Implement more of these!
        // Unrecognized key combination.
        int UNKNOWN = 0;

        // Tab/window creation.
        int OPEN_RECENTLY_CLOSED_TAB = 1;
        int OPEN_NEW_TAB = 2;
        int OPEN_NEW_TAB_INCOGNITO = 3;
        int OPEN_NEW_WINDOW = 4;

        // Tab control.
        int RELOAD_TAB = 5;
        int NOT_IMPLEMENTED_RELOAD_TAB_BYPASSING_CACHE = 6;
        int MOVE_TO_TAB_LEFT = 7;
        int MOVE_TO_TAB_RIGHT = 8;
        int MOVE_TO_SPECIFIC_TAB = 9;
        int MOVE_TO_LAST_TAB = 10;
        int NOT_IMPLEMENTED_TAB_SEARCH = 11;
        int NOT_IMPLEMENTED_TOGGLE_MULTITASK_MENU = 12;

        // Closing.
        int CLOSE_TAB = 13;
        int NOT_IMPLEMENTED_CLOSE_WINDOW = 14;
        int NOT_IMPLEMENTED_QUIT_CHROME = 15;

        // Navigation controls.
        int JUMP_TO_OMNIBOX = 16;
        int JUMP_TO_SEARCH = 17;
        int NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE = 18;
        int NOT_IMPLEMENTED_SCROLL_DOWN = 19;
        int NOT_IMPLEMENTED_SCROLL_UP = 20;

        // Top controls.
        int NOT_IMPLEMENTED_KEYBOARD_FOCUS_TOOLBAR = 21;
        int NOT_IMPLEMENTED_KEYBOARD_FOCUS_BOOKMARKS = 22;
        int NOT_IMPLEMENTED_KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS = 23;
        int NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU = 24;
        int NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT = 25;
        int NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT = 26;
        int NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT = 27;
        int NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT = 28;

        // Accessibility.
        int NOT_IMPLEMENTED_TOGGLE_CARET_BROWSING = 29;
        int NOT_IMPLEMENTED_FOCUS_ON_INACTIVE_DIALOGS = 30;

        // Bookmarks.
        int OPEN_BOOKMARKS = 31;
        int BOOKMARK_PAGE = 32;
        int NOT_IMPLEMENTED_BOOKMARK_ALL_TABS = 33;
        int TOGGLE_BOOKMARK_BAR = 34;

        // Fullscreen.
        int NOT_IMPLEMENTED_TOGGLE_IMMERSIVE = 35;
        int NOT_IMPLEMENTED_EXIT_IMMERSIVE = 36;

        // Developer tools.
        int DEV_TOOLS = 37;
        int NOT_IMPLEMENTED_DEV_TOOLS_CONSOLE = 38;
        int NOT_IMPLEMENTED_DEV_TOOLS_INSPECT = 39;
        int NOT_IMPLEMENTED_DEV_TOOLS_TOGGLE = 40;
        int NOT_IMPLEMENTED_VIEW_SOURCE = 41;
        int NOT_IMPLEMENTED_TASK_MANAGER = 42;

        // Downloads.
        int SAVE_PAGE = 43;
        int NOT_IMPLEMENTED_SHOW_DOWNLOADS = 44;

        // History.
        int OPEN_HISTORY = 45;
        int HISTORY_GO_BACK = 46;
        int HISTORY_GO_FORWARD = 47;
        int NOT_IMPLEMENTED_CLEAR_BROWSING_DATA = 48;

        // Print.
        int PRINT = 49;
        int NOT_IMPLEMENTED_BASIC_PRINT = 50;

        // Zoom controls.
        int ZOOM_IN = 51;
        int ZOOM_OUT = 52;
        int ZOOM_RESET = 53;

        // Misc.
        int NOT_IMPLEMENTED_AVATAR_MENU = 54;
        int NOT_IMPLEMENTED_FEEDBACK_FORM = 55;
        int FIND_IN_PAGE = 56;
        int NOT_IMPLEMENTED_HOME = 57;
        int OPEN_HELP = 58;
        int OPEN_MENU = 59;

        // Custom keyboard shortcut used by extensions.
        // This enum isn't precisely a single semantic meaning, but we want to report metrics.
        int CUSTOM_EXTENSION_SHORTCUT = 60;

        // Max value.
        int MAX_VALUE = 61;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:KeyboardShortcutsSemanticMeaning, //tools/metrics/histograms/metadata/accessibility/histograms.xml:KeyboardShortcutsSemanticMeaning)

    private static @KeyboardShortcutsSemanticMeaning int getKeyboardSemanticMeaning(
            int keyCodeAndMeta) {
        switch (keyCodeAndMeta) {
                // Tab/window creation.
            case CTRL | SHIFT | KeyEvent.KEYCODE_T:
                return KeyboardShortcutsSemanticMeaning.OPEN_RECENTLY_CLOSED_TAB;
            case CTRL | KeyEvent.KEYCODE_T:
                return KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB;
            case CTRL | SHIFT | KeyEvent.KEYCODE_N:
                return KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB_INCOGNITO;
            case CTRL | KeyEvent.KEYCODE_N:
                return KeyboardShortcutsSemanticMeaning.OPEN_NEW_WINDOW;

                // Tab control.
            case CTRL | SHIFT | KeyEvent.KEYCODE_R:
            case CTRL | KeyEvent.KEYCODE_R:
            case SHIFT | KeyEvent.KEYCODE_F5:
            case KeyEvent.KEYCODE_F5:
            case KeyEvent.KEYCODE_REFRESH:
                return KeyboardShortcutsSemanticMeaning.RELOAD_TAB;
            case CTRL | KeyEvent.KEYCODE_F5:
            case CTRL | KeyEvent.KEYCODE_REFRESH:
            case SHIFT | KeyEvent.KEYCODE_REFRESH:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_RELOAD_TAB_BYPASSING_CACHE;
            case CTRL | SHIFT | KeyEvent.KEYCODE_TAB:
            case CTRL | KeyEvent.KEYCODE_PAGE_UP:
            case KeyEvent.KEYCODE_BUTTON_L1:
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT;
            case CTRL | KeyEvent.KEYCODE_TAB:
            case CTRL | KeyEvent.KEYCODE_PAGE_DOWN:
            case KeyEvent.KEYCODE_BUTTON_R1:
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT;
            case CTRL | KeyEvent.KEYCODE_1:
            case CTRL | KeyEvent.KEYCODE_2:
            case CTRL | KeyEvent.KEYCODE_3:
            case CTRL | KeyEvent.KEYCODE_4:
            case CTRL | KeyEvent.KEYCODE_5:
            case CTRL | KeyEvent.KEYCODE_6:
            case CTRL | KeyEvent.KEYCODE_7:
            case CTRL | KeyEvent.KEYCODE_8:
            case ALT | KeyEvent.KEYCODE_1:
            case ALT | KeyEvent.KEYCODE_2:
            case ALT | KeyEvent.KEYCODE_3:
            case ALT | KeyEvent.KEYCODE_4:
            case ALT | KeyEvent.KEYCODE_5:
            case ALT | KeyEvent.KEYCODE_6:
            case ALT | KeyEvent.KEYCODE_7:
            case ALT | KeyEvent.KEYCODE_8:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_1:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_2:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_3:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_4:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_5:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_6:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_7:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_8:
            case ALT | KeyEvent.KEYCODE_NUMPAD_1:
            case ALT | KeyEvent.KEYCODE_NUMPAD_2:
            case ALT | KeyEvent.KEYCODE_NUMPAD_3:
            case ALT | KeyEvent.KEYCODE_NUMPAD_4:
            case ALT | KeyEvent.KEYCODE_NUMPAD_5:
            case ALT | KeyEvent.KEYCODE_NUMPAD_6:
            case ALT | KeyEvent.KEYCODE_NUMPAD_7:
            case ALT | KeyEvent.KEYCODE_NUMPAD_8:
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB;
            case CTRL | KeyEvent.KEYCODE_9:
            case ALT | KeyEvent.KEYCODE_9:
            case CTRL | KeyEvent.KEYCODE_NUMPAD_9:
            case ALT | KeyEvent.KEYCODE_NUMPAD_9:
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB;
            case CTRL | SHIFT | KeyEvent.KEYCODE_A:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TAB_SEARCH;
                // TODO(crbug.com/402775002): Figure out what shortcut does TOGGLE_MULTITASK_MENU.

                // Closing.
            case CTRL | KeyEvent.KEYCODE_W:
            case CTRL | KeyEvent.KEYCODE_F4:
            case KeyEvent.KEYCODE_BUTTON_B:
                return KeyboardShortcutsSemanticMeaning.CLOSE_TAB;
            case CTRL | SHIFT | KeyEvent.KEYCODE_W:
            case ALT | KeyEvent.KEYCODE_F4:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CLOSE_WINDOW;
                // TODO(crbug.com/402775002): Change fn signature to allow (Alt + F then X) or
                // Command+Q

                // Navigation controls.
            case CTRL | KeyEvent.KEYCODE_L:
            case ALT | KeyEvent.KEYCODE_D:
            case KeyEvent.KEYCODE_BUTTON_X:
                return KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX;
            case CTRL | KeyEvent.KEYCODE_E:
                return KeyboardShortcutsSemanticMeaning.JUMP_TO_SEARCH;
            case CTRL | KeyEvent.KEYCODE_F6:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE;
            case KeyEvent.KEYCODE_SPACE:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_DOWN;
            case SHIFT | KeyEvent.KEYCODE_SPACE:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_UP;

                // Top controls.
            case ALT | SHIFT | KeyEvent.KEYCODE_T:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_TOOLBAR;
            case ALT | SHIFT | KeyEvent.KEYCODE_B:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_BOOKMARKS;
            case KeyEvent.KEYCODE_F6:
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS;
            case SHIFT | KeyEvent.KEYCODE_F7:
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU;
            case CTRL | KeyEvent.KEYCODE_DPAD_LEFT:
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT;
            case CTRL | KeyEvent.KEYCODE_DPAD_RIGHT:
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT;
            case CTRL | SHIFT | KeyEvent.KEYCODE_PAGE_UP:
                // TODO(crbug.com/402775002): Change fn signature to allow CTRL+SHIFT+FN+UpArrow.
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT;
            case CTRL | SHIFT | KeyEvent.KEYCODE_PAGE_DOWN:
                // TODO(crbug.com/402775002): Change fn signature to allow CTRL+SHIFT+FN+DownArrow.
                return KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT;

                // Accessibility.
            case CTRL | KeyEvent.KEYCODE_F7:
            case KeyEvent.KEYCODE_F7:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_CARET_BROWSING;
            case ALT | SHIFT | KeyEvent.KEYCODE_A:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_ON_INACTIVE_DIALOGS;

                // Bookmarks.
            case CTRL | SHIFT | KeyEvent.KEYCODE_O:
                return KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS;
            case KeyEvent.KEYCODE_BOOKMARK:
            case CTRL | KeyEvent.KEYCODE_D:
                return KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE;
            case CTRL | SHIFT | KeyEvent.KEYCODE_D:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BOOKMARK_ALL_TABS;
            case CTRL | SHIFT | KeyEvent.KEYCODE_B:
                return KeyboardShortcutsSemanticMeaning.TOGGLE_BOOKMARK_BAR;

                // Fullscreen.
            case KeyEvent.KEYCODE_F11:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_IMMERSIVE;
                // TODO(crbug.com/402775002): Allow long press on Esc.

                // Developer tools.
            case CTRL | SHIFT | KeyEvent.KEYCODE_I:
                return KeyboardShortcutsSemanticMeaning.DEV_TOOLS;
            case CTRL | SHIFT | KeyEvent.KEYCODE_J:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_CONSOLE;
            case CTRL | SHIFT | KeyEvent.KEYCODE_C:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_INSPECT;
            case KeyEvent.KEYCODE_F12:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_TOGGLE;
            case CTRL | KeyEvent.KEYCODE_U:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_VIEW_SOURCE;
            case SHIFT | KeyEvent.KEYCODE_ESCAPE:
                // TODO(crbug.com/402775002): Change fn signature to allow Command+Esc.
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TASK_MANAGER;

                // Downloads.
            case CTRL | KeyEvent.KEYCODE_S:
                return KeyboardShortcutsSemanticMeaning.SAVE_PAGE;
            case CTRL | KeyEvent.KEYCODE_J:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SHOW_DOWNLOADS;

                // History.
            case CTRL | KeyEvent.KEYCODE_H:
                return KeyboardShortcutsSemanticMeaning.OPEN_HISTORY;
            case ALT | KeyEvent.KEYCODE_DPAD_LEFT:
                return KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK;
            case ALT | KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_FORWARD:
            case KeyEvent.KEYCODE_BUTTON_START:
                return KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD;
            case CTRL | SHIFT | KeyEvent.KEYCODE_DEL:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CLEAR_BROWSING_DATA;

                // Print.
            case CTRL | KeyEvent.KEYCODE_P:
                return KeyboardShortcutsSemanticMeaning.PRINT;
            case CTRL | SHIFT | KeyEvent.KEYCODE_P:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BASIC_PRINT;

                // Zoom controls.
            case CTRL | KeyEvent.KEYCODE_PLUS:
            case CTRL | KeyEvent.KEYCODE_EQUALS:
            case CTRL | SHIFT | KeyEvent.KEYCODE_PLUS:
            case CTRL | SHIFT | KeyEvent.KEYCODE_EQUALS:
            case KeyEvent.KEYCODE_ZOOM_IN:
                return KeyboardShortcutsSemanticMeaning.ZOOM_IN;
            case CTRL | KeyEvent.KEYCODE_MINUS:
            case KeyEvent.KEYCODE_ZOOM_OUT:
                return KeyboardShortcutsSemanticMeaning.ZOOM_OUT;
            case CTRL | KeyEvent.KEYCODE_0:
                return KeyboardShortcutsSemanticMeaning.ZOOM_RESET;

                // Misc.
            case CTRL | SHIFT | KeyEvent.KEYCODE_M:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_AVATAR_MENU;
            case ALT | SHIFT | KeyEvent.KEYCODE_I:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FEEDBACK_FORM;
            case CTRL | KeyEvent.KEYCODE_F:
            case CTRL | KeyEvent.KEYCODE_G:
            case CTRL | SHIFT | KeyEvent.KEYCODE_G:
            case KeyEvent.KEYCODE_F3:
            case SHIFT | KeyEvent.KEYCODE_F3:
                return KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE;
            case ALT | KeyEvent.KEYCODE_HOME:
            case KeyEvent.KEYCODE_HOME:
                return KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_HOME;
            case CTRL | SHIFT | KeyEvent.KEYCODE_SLASH: // i.e. Ctrl+?
            case KeyEvent.KEYCODE_F1:
                return KeyboardShortcutsSemanticMeaning.OPEN_HELP;
            case ALT | KeyEvent.KEYCODE_E:
            case ALT | KeyEvent.KEYCODE_F:
            case KeyEvent.KEYCODE_F10:
            case KeyEvent.KEYCODE_BUTTON_Y:
                return KeyboardShortcutsSemanticMeaning.OPEN_MENU;
        }

        return KeyboardShortcutsSemanticMeaning.UNKNOWN;
    }

    private static int getMetaState(KeyEvent event) {
        return (event.isCtrlPressed() ? CTRL : 0)
                | (event.isAltPressed() ? ALT : 0)
                | (event.isShiftPressed() ? SHIFT : 0);
    }

    /**
     * This should be called from the Activity's dispatchKeyEvent() to handle keyboard shortcuts.
     *
     * <p>Note: dispatchKeyEvent() is called before the active view or web page gets a chance to
     * handle the key event. So the keys handled here cannot be overridden by any view or web page.
     *
     * @param event The KeyEvent to handle.
     * @param uiInitialized Whether the UI has been initialized. If this is false, most keys will
     *     not be handled.
     * @param fullscreenManager Manages fullscreen state.
     * @param menuOrKeyboardActionController Controls keyboard actions.
     * @param context The android context.
     * @return True if the event was handled. False if the event was ignored. Null if the event
     *     should be handled by the activity's parent class.
     */
    public static Boolean dispatchKeyEvent(
            KeyEvent event,
            boolean uiInitialized,
            FullscreenManager fullscreenManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Context context) {
        int keyCode = event.getKeyCode();
        if (!uiInitialized) {
            if (keyCode == KeyEvent.KEYCODE_SEARCH || keyCode == KeyEvent.KEYCODE_MENU) return true;
            return null;
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_SEARCH:
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.focus_url_bar, false);
                }
                // Always consume the SEARCH key events to prevent android from showing
                // the default app search UI, which locks up Chrome.
                return true;
            case KeyEvent.KEYCODE_MENU:
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.show_menu, false);
                }
                return true;
            case KeyEvent.KEYCODE_ESCAPE:
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    if (fullscreenManager.getPersistentFullscreenMode()) {
                        fullscreenManager.exitPersistentFullscreenMode();
                        return true;
                    }
                    if (getMetaState(event) == CTRL
                            && ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_CLANK)) {
                        TaskManager taskManager = TaskManagerFactory.createTaskManager();
                        taskManager.launch(context);
                        return true;
                    }
                }
                break;
            case KeyEvent.KEYCODE_TV:
            case KeyEvent.KEYCODE_GUIDE:
            case KeyEvent.KEYCODE_DVR:
            case KeyEvent.KEYCODE_AVR_INPUT:
            case KeyEvent.KEYCODE_AVR_POWER:
            case KeyEvent.KEYCODE_STB_INPUT:
            case KeyEvent.KEYCODE_STB_POWER:
            case KeyEvent.KEYCODE_TV_INPUT:
            case KeyEvent.KEYCODE_TV_POWER:
            case KeyEvent.KEYCODE_WINDOW:
                // Do not consume the AV device-related keys so that the system will take
                // an appropriate action, such as switching to TV mode.
                return false;
        }

        return null;
    }

    /**
     * This method should be called when overriding from
     * {@link android.app.Activity#onProvideKeyboardShortcuts(List, android.view.Menu, int)}
     * in an activity. It will return a list of the possible shortcuts. If
     * someone adds a shortcut they also need to add an explanation in the
     * appropriate group in this method so the user can see it when this method
     * is called.
     *
     * Preventing inlining since this uses APIs only available on Android N, and this causes dex
     * validation failures on earlier versions if inlined.
     *
     * @param context We need an activity so we can call the strings from our
     *            resource.
     * @return a list of shortcuts organized into groups.
     */
    public static List<KeyboardShortcutGroup> createShortcutGroup(Context context) {
        final int ctrlShift = KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON;

        List<KeyboardShortcutGroup> shortcutGroups = new ArrayList<>();

        KeyboardShortcutGroup tabShortcutGroup =
                new KeyboardShortcutGroup(
                        context.getString(R.string.keyboard_shortcut_tab_group_header));
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_open_new_tab,
                KeyEvent.KEYCODE_T,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_open_new_window,
                KeyEvent.KEYCODE_N,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_reopen_new_tab,
                KeyEvent.KEYCODE_T,
                ctrlShift);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_new_incognito_tab,
                KeyEvent.KEYCODE_N,
                ctrlShift);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_next_tab,
                KeyEvent.KEYCODE_TAB,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_prev_tab,
                KeyEvent.KEYCODE_TAB,
                ctrlShift);
        addShortcut(
                context,
                tabShortcutGroup,
                R.string.keyboard_shortcut_close_tab,
                KeyEvent.KEYCODE_W,
                KeyEvent.META_CTRL_ON);
        shortcutGroups.add(tabShortcutGroup);

        KeyboardShortcutGroup chromeFeatureShortcutGroup =
                new KeyboardShortcutGroup(
                        context.getString(R.string.keyboard_shortcut_chrome_feature_group_header));
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_open_menu,
                KeyEvent.KEYCODE_E,
                KeyEvent.META_ALT_ON);
        if (BookmarkBarUtils.isFeatureEnabled(context)) {
            addShortcut(
                    context,
                    chromeFeatureShortcutGroup,
                    R.string.keyboard_shortcut_toggle_bookmark_bar,
                    KeyEvent.KEYCODE_B,
                    ctrlShift);
        }
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_bookmark_manager,
                KeyEvent.KEYCODE_O,
                ctrlShift);
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_history_manager,
                KeyEvent.KEYCODE_H,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_find_bar,
                KeyEvent.KEYCODE_F,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_address_bar,
                KeyEvent.KEYCODE_L,
                KeyEvent.META_CTRL_ON);
        shortcutGroups.add(chromeFeatureShortcutGroup);

        KeyboardShortcutGroup webpageShortcutGroup =
                new KeyboardShortcutGroup(
                        context.getString(R.string.keyboard_shortcut_webpage_group_header));
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_print_page,
                KeyEvent.KEYCODE_P,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_reload_page,
                KeyEvent.KEYCODE_R,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_reload_no_cache,
                KeyEvent.KEYCODE_R,
                ctrlShift);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_bookmark_page,
                KeyEvent.KEYCODE_D,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_zoom_in,
                KeyEvent.KEYCODE_EQUALS,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_zoom_out,
                KeyEvent.KEYCODE_MINUS,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_reset_zoom,
                KeyEvent.KEYCODE_0,
                KeyEvent.META_CTRL_ON);
        addShortcut(
                context,
                webpageShortcutGroup,
                R.string.keyboard_shortcut_help_center,
                KeyEvent.KEYCODE_SLASH,
                ctrlShift);
        shortcutGroups.add(webpageShortcutGroup);

        if (DeviceFormFactor.isDesktop()) {
            KeyboardShortcutGroup developerShortcutGroup =
                    new KeyboardShortcutGroup(
                            context.getString(R.string.keyboard_shortcut_developer_group_header));
            addShortcut(
                    context,
                    developerShortcutGroup,
                    R.string.keyboard_shortcut_developer_tools,
                    KeyEvent.KEYCODE_I,
                    ctrlShift);
            shortcutGroups.add(developerShortcutGroup);
        }

        return shortcutGroups;
    }

    private static void addShortcut(
            Context context,
            KeyboardShortcutGroup shortcutGroup,
            int resId,
            int keyCode,
            int keyModifier) {
        shortcutGroup.addItem(
                new KeyboardShortcutInfo(context.getString(resId), keyCode, keyModifier));
    }

    /**
     * This should be called from the Activity's onKeyDown() to handle keyboard shortcuts.
     *
     * <p>Note: onKeyDown() is called after the active view or web page has had a chance to handle
     * the key event. So the keys handled here *can* be overridden by any view or web page.
     *
     * @param event The KeyEvent to handle.
     * @param isCurrentTabVisible Whether page-related actions are valid, e.g. reload, zoom in. This
     *     should be false when in the tab switcher.
     * @param tabSwitchingEnabled Whether shortcuts that switch between tabs are enabled (e.g.
     *     Ctrl+Tab, Ctrl+3).
     * @param tabModelSelector The current tab modelSelector.
     * @param menuOrKeyboardActionController Controls keyboard actions.
     * @param toolbarManager Manages the toolbar.
     * @return Whether the key event was handled.
     */
    public static boolean onKeyDown(
            KeyEvent event,
            boolean isCurrentTabVisible,
            boolean tabSwitchingEnabled,
            TabModelSelector tabModelSelector,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            ToolbarManager toolbarManager) {
        int keyCode = event.getKeyCode();
        if (event.getRepeatCount() != 0 || KeyEvent.isModifierKey(keyCode)) return false;
        if (KeyEvent.isGamepadButton(keyCode)) {
            if (GamepadList.isGamepadAPIActive()) return false;
        } else if (!event.isCtrlPressed()
                && !event.isAltPressed()
                && keyCode != KeyEvent.KEYCODE_F3
                && keyCode != KeyEvent.KEYCODE_F5
                && keyCode != KeyEvent.KEYCODE_F6
                && keyCode != KeyEvent.KEYCODE_F10
                && keyCode != KeyEvent.KEYCODE_FORWARD
                && keyCode != KeyEvent.KEYCODE_REFRESH) {
            return false;
        }

        TabModel currentTabModel = tabModelSelector.getCurrentModel();
        Tab currentTab = tabModelSelector.getCurrentTab();
        WebContents currentWebContents = currentTab == null ? null : currentTab.getWebContents();

        int tabCount = currentTabModel.getCount();
        int metaState = getMetaState(event);
        int keyCodeAndMeta = keyCode | metaState;
        @KeyboardShortcutsSemanticMeaning
        int semanticMeaning = getKeyboardSemanticMeaning(keyCodeAndMeta);

        RecordHistogram.recordEnumeratedHistogram(
                AccessibilityState.isScreenReaderEnabled()
                        ? "Accessibility.Android.KeyboardShortcut.ScreenReaderRunning2"
                        : "Accessibility.Android.KeyboardShortcut.NoScreenReader2",
                semanticMeaning,
                KeyboardShortcuts.KeyboardShortcutsSemanticMeaning.MAX_VALUE);

        switch (semanticMeaning) {
            case KeyboardShortcutsSemanticMeaning.OPEN_RECENTLY_CLOSED_TAB:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        R.id.open_recently_closed_tab, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        currentTabModel.isIncognito()
                                ? R.id.new_incognito_tab_menu_id
                                : R.id.new_tab_menu_id,
                        false);
                return true;
            case KeyboardShortcutsSemanticMeaning.OPEN_NEW_WINDOW:
                if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.new_window_menu_id, false);
                    return true;
                } else {
                    break;
                }
            case KeyboardShortcutsSemanticMeaning.DEV_TOOLS:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.dev_tools, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.SAVE_PAGE:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.offline_page_id, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB_INCOGNITO:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        R.id.new_incognito_tab_menu_id, false);
                return true;
                // Alt+E represents a special character ´ (latin code: &#180) in Android.
                // If an EditText or ContentView has focus, Alt+E will be swallowed by
                // the default dispatchKeyEvent and cannot open the menu.
            case KeyboardShortcutsSemanticMeaning.OPEN_MENU:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.show_menu, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.TOGGLE_BOOKMARK_BAR:
                return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        R.id.toggle_bookmark_bar, /* fromMenu= */ false);
        }

        if (isCurrentTabVisible) {
            switch (semanticMeaning) {
                case KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB:
                    if (tabSwitchingEnabled) {
                        int numCode =
                                (KeyEvent.KEYCODE_1 <= keyCode && keyCode <= KeyEvent.KEYCODE_8)
                                        ? keyCode - KeyEvent.KEYCODE_0
                                        : keyCode - KeyEvent.KEYCODE_NUMPAD_0;
                        // Keep this condition to make sure that:
                        // 1) If we're not using keyboard number keys, we're still in the right
                        // bounds for numpad keys
                        // 2) We don't try to set the tab model index out of bounds.
                        if (numCode > 0 && numCode <= Math.min(tabCount, 8)) {
                            TabModelUtils.setIndex(currentTabModel, numCode - 1);
                        }
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB:
                    if (tabSwitchingEnabled && tabCount != 0) {
                        TabModelUtils.setIndex(currentTabModel, tabCount - 1);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT:
                    if (tabSwitchingEnabled && tabCount > 1) {
                        TabModelUtils.setIndex(
                                currentTabModel, (currentTabModel.index() + 1) % tabCount);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT:
                    if (tabSwitchingEnabled && tabCount > 1) {
                        TabModelUtils.setIndex(
                                currentTabModel,
                                (currentTabModel.index() + tabCount - 1) % tabCount);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.CLOSE_TAB:
                    Tab tab = TabModelUtils.getCurrentTab(currentTabModel);
                    if (tab != null) {
                        currentTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(true).build(),
                                        /* allowDialog= */ true);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.find_in_page_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.focus_url_bar, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.JUMP_TO_SEARCH:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.focus_and_clear_url_bar, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.all_bookmarks_menu_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.bookmark_this_page_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.OPEN_HISTORY:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.open_history_menu_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.PRINT:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.print_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.ZOOM_IN:
                    ZoomController.zoomIn(currentWebContents);
                    return true;
                case KeyboardShortcutsSemanticMeaning.ZOOM_OUT:
                    ZoomController.zoomOut(currentWebContents);
                    return true;
                case KeyboardShortcutsSemanticMeaning.ZOOM_RESET:
                    ZoomController.zoomReset(currentWebContents);
                    return true;
                case KeyboardShortcutsSemanticMeaning.RELOAD_TAB:
                    if (currentTab != null) {
                        if ((keyCodeAndMeta & SHIFT) == SHIFT) {
                            currentTab.reloadIgnoringCache();
                        } else {
                            currentTab.reload();
                        }

                        if (toolbarManager != null
                                && currentWebContents != null
                                && currentWebContents.focusLocationBarByDefault()) {
                            toolbarManager.revertLocationBarChanges();
                        } else if (currentTab.getView() != null) {
                            currentTab.getView().requestFocus();
                        }
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK:
                    if (currentTab != null && currentTab.canGoBack()) currentTab.goBack();
                    return true;
                case KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD:
                    if (currentTab != null && currentTab.canGoForward()) currentTab.goForward();
                    return true;
                case KeyboardShortcutsSemanticMeaning.OPEN_HELP:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.help_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning
                        .NOT_IMPLEMENTED_KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS:
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_KEYBOARD_A11Y)) {
                        // TODO(crbug.com/360423850): Don't allow F6 to be overridden by websites.
                        return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                                R.id.switch_keyboard_focus_row, /* fromMenu= */ false);
                    } else {
                        return false;
                    }
                case KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_TOOLBAR:
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_KEYBOARD_A11Y)) {
                        toolbarManager.requestFocus();
                        return true;
                    } else {
                        return false;
                    }
                case KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_KEYBOARD_FOCUS_BOOKMARKS:
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_KEYBOARD_A11Y)) {
                        return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                                R.id.focus_bookmarks, /* fromMenu= */ false);
                    } else {
                        return false;
                    }
            }
        }

        return false;
    }
}
