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
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.ArrayList;
import java.util.List;

/** Implements app-level keyboard shortcuts for ChromeTabbedActivity and DocumentActivity. */
public class KeyboardShortcuts {

    private static final int CTRL = 1 << 31;
    private static final int ALT = 1 << 30;
    private static final int SHIFT = 1 << 29;

    private KeyboardShortcuts() {}

    // KeyboardShortcutsSemanticMeaning defined in tools/metrics/histograms/enums.xml.
    // Add new values before MAX_VALUE. These values are persisted to logs. Entries should not be
    // renumbered and numeric values should never be reused.
    // LINT.IfChange(KeyboardShortcutsSemanticMeaning)
    @IntDef({
        KeyboardShortcutsSemanticMeaning.UNKNOWN,
        KeyboardShortcutsSemanticMeaning.OPEN_RECENTLY_CLOSED_TAB,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB_INCOGNITO,
        KeyboardShortcutsSemanticMeaning.OPEN_NEW_WINDOW,
        KeyboardShortcutsSemanticMeaning.RELOAD_TAB,
        KeyboardShortcutsSemanticMeaning.CLOSE_TAB,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB,
        KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX,
        KeyboardShortcutsSemanticMeaning.GO_BACK,
        KeyboardShortcutsSemanticMeaning.GO_FORWARD,
        KeyboardShortcutsSemanticMeaning.OPEN_MENU,
        KeyboardShortcutsSemanticMeaning.OPEN_HELP,
        KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE,
        KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS,
        KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE,
        KeyboardShortcutsSemanticMeaning.OPEN_HISTORY,
        KeyboardShortcutsSemanticMeaning.SAVE_PAGE,
        KeyboardShortcutsSemanticMeaning.PRINT,
        KeyboardShortcutsSemanticMeaning.ZOOM_IN,
        KeyboardShortcutsSemanticMeaning.ZOOM_OUT,
        KeyboardShortcutsSemanticMeaning.ZOOM_RESET,
        KeyboardShortcutsSemanticMeaning.MAX_VALUE
    })
    public @interface KeyboardShortcutsSemanticMeaning {
        // Unrecognized key combination.
        int UNKNOWN = 0;

        // Tab/window creation.
        int OPEN_RECENTLY_CLOSED_TAB = 1;
        int OPEN_NEW_TAB = 2;
        int OPEN_NEW_TAB_INCOGNITO = 3;
        int OPEN_NEW_WINDOW = 4;

        // Tab control.
        int RELOAD_TAB = 5;
        int CLOSE_TAB = 6;
        int MOVE_TO_TAB_LEFT = 7;
        int MOVE_TO_TAB_RIGHT = 8;
        int MOVE_TO_SPECIFIC_TAB = 9;
        int MOVE_TO_LAST_TAB = 10;

        // Navigation controls.
        int JUMP_TO_OMNIBOX = 11;
        int GO_BACK = 12;
        int GO_FORWARD = 13;

        // 3-dot menu controls.
        int OPEN_MENU = 14;
        int OPEN_HELP = 15;
        int FIND_IN_PAGE = 16;
        int OPEN_BOOKMARKS = 17;
        int BOOKMARK_PAGE = 18;
        int OPEN_HISTORY = 19;
        int SAVE_PAGE = 20;
        int PRINT = 21;

        // Zoom controls.
        int ZOOM_IN = 22;
        int ZOOM_OUT = 23;
        int ZOOM_RESET = 24;

        // Be sure to also update enums.xml when updating these values.
        int MAX_VALUE = 25;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:KeyboardShortcutsSemanticMeaning)

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
            case CTRL | KeyEvent.KEYCODE_W:
            case CTRL | KeyEvent.KEYCODE_F4:
            case KeyEvent.KEYCODE_BUTTON_B:
                return KeyboardShortcutsSemanticMeaning.CLOSE_TAB;
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
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB;
            case CTRL | KeyEvent.KEYCODE_9:
            case ALT | KeyEvent.KEYCODE_9:
                return KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB;

                // Navigation controls.
            case CTRL | KeyEvent.KEYCODE_L:
            case ALT | KeyEvent.KEYCODE_D:
            case KeyEvent.KEYCODE_BUTTON_X:
                return KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX;
            case ALT | KeyEvent.KEYCODE_DPAD_LEFT:
                return KeyboardShortcutsSemanticMeaning.GO_BACK;
            case ALT | KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_FORWARD:
            case KeyEvent.KEYCODE_BUTTON_START:
                return KeyboardShortcutsSemanticMeaning.GO_FORWARD;

                // 3-dot menu controls.
            case ALT | KeyEvent.KEYCODE_E:
            case ALT | KeyEvent.KEYCODE_F:
            case KeyEvent.KEYCODE_F10:
            case KeyEvent.KEYCODE_BUTTON_Y:
                return KeyboardShortcutsSemanticMeaning.OPEN_MENU;
            case CTRL | SHIFT | KeyEvent.KEYCODE_SLASH: // i.e. Ctrl+?
                return KeyboardShortcutsSemanticMeaning.OPEN_HELP;
            case CTRL | KeyEvent.KEYCODE_F:
            case CTRL | KeyEvent.KEYCODE_G:
            case CTRL | SHIFT | KeyEvent.KEYCODE_G:
            case KeyEvent.KEYCODE_F3:
            case SHIFT | KeyEvent.KEYCODE_F3:
                return KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE;
            case CTRL | SHIFT | KeyEvent.KEYCODE_B:
                return KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS;
            case KeyEvent.KEYCODE_BOOKMARK:
            case CTRL | KeyEvent.KEYCODE_D:
                return KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE;
            case CTRL | KeyEvent.KEYCODE_H:
                return KeyboardShortcutsSemanticMeaning.OPEN_HISTORY;
            case CTRL | KeyEvent.KEYCODE_S:
                return KeyboardShortcutsSemanticMeaning.SAVE_PAGE;
            case CTRL | KeyEvent.KEYCODE_P:
                return KeyboardShortcutsSemanticMeaning.PRINT;

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
     * Note: dispatchKeyEvent() is called before the active view or web page gets a chance to handle
     * the key event. So the keys handled here cannot be overridden by any view or web page.
     *
     * @param event The KeyEvent to handle.
     * @param uiInitialized Whether the UI has been initialized. If this is false, most keys will
     *                      not be handled.
     * @param fullscreenManager Manages fullscreen state.
     * @param menuOrKeyboardActionController Controls keyboard actions.
     * @return True if the event was handled. False if the event was ignored. Null if the event
     *         should be handled by the activity's parent class.
     */
    public static Boolean dispatchKeyEvent(
            KeyEvent event,
            boolean uiInitialized,
            FullscreenManager fullscreenManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
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
        addShortcut(
                context,
                chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_bookmark_manager,
                KeyEvent.KEYCODE_B,
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
     * Note: onKeyDown() is called after the active view or web page has had a chance to handle
     * the key event. So the keys handled here *can* be overridden by any view or web page.
     *
     * @param event The KeyEvent to handle.
     * @param isCurrentTabVisible Whether page-related actions are valid, e.g. reload, zoom in. This
     *         should be false when in the tab switcher.
     * @param tabSwitchingEnabled Whether shortcuts that switch between tabs are enabled (e.g.
     *         Ctrl+Tab, Ctrl+3).
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
                        ? "Accessibility.Android.KeyboardShortcut.ScreenReaderRunning"
                        : "Accessibility.Android.KeyboardShortcut.NoScreenReader",
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
        }

        if (isCurrentTabVisible) {
            switch (semanticMeaning) {
                case KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB:
                    if (tabSwitchingEnabled) {
                        int numCode = keyCode - KeyEvent.KEYCODE_0;
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
                    TabModelUtils.closeCurrentTab(currentTabModel);
                    return true;
                case KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.find_in_page_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.focus_url_bar, false);
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
                case KeyboardShortcutsSemanticMeaning.GO_BACK:
                    if (currentTab != null && currentTab.canGoBack()) currentTab.goBack();
                    return true;
                case KeyboardShortcutsSemanticMeaning.GO_FORWARD:
                    if (currentTab != null && currentTab.canGoForward()) currentTab.goForward();
                    return true;
                case KeyboardShortcutsSemanticMeaning.OPEN_HELP:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.help_id, false);
                    return true;
            }
        }

        return false;
    }
}
