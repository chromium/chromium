// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.os.Build;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;

import androidx.annotation.RequiresApi;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.ArrayList;
import java.util.List;

/**
 * Implements app-level keyboard shortcuts for ChromeTabbedActivity and DocumentActivity.
 */
public class KeyboardShortcuts {

    private static final int CTRL = 1 << 31;
    private static final int ALT = 1 << 30;
    private static final int SHIFT = 1 << 29;

    private KeyboardShortcuts() {}

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
    public static Boolean dispatchKeyEvent(KeyEvent event, boolean uiInitialized,
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
    @RequiresApi(Build.VERSION_CODES.N)
    public static List<KeyboardShortcutGroup> createShortcutGroup(Context context) {
        final int ctrlShift = KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON;

        List<KeyboardShortcutGroup> shortcutGroups = new ArrayList<>();

        KeyboardShortcutGroup tabShortcutGroup = new KeyboardShortcutGroup(
                context.getString(R.string.keyboard_shortcut_tab_group_header));
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_open_new_tab,
                KeyEvent.KEYCODE_N, KeyEvent.META_CTRL_ON);
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_reopen_new_tab,
                KeyEvent.KEYCODE_T, ctrlShift);
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_new_incognito_tab,
                KeyEvent.KEYCODE_N, ctrlShift);
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_next_tab,
                KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON);
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_prev_tab,
                KeyEvent.KEYCODE_TAB, ctrlShift);
        addShortcut(context, tabShortcutGroup, R.string.keyboard_shortcut_close_tab,
                KeyEvent.KEYCODE_W, KeyEvent.META_CTRL_ON);
        shortcutGroups.add(tabShortcutGroup);

        KeyboardShortcutGroup chromeFeatureShortcutGroup = new KeyboardShortcutGroup(
                context.getString(R.string.keyboard_shortcut_chrome_feature_group_header));
        addShortcut(context, chromeFeatureShortcutGroup, R.string.keyboard_shortcut_open_menu,
                KeyEvent.KEYCODE_E, KeyEvent.META_ALT_ON);
        addShortcut(context, chromeFeatureShortcutGroup,
                R.string.keyboard_shortcut_bookmark_manager, KeyEvent.KEYCODE_B, ctrlShift);
        addShortcut(context, chromeFeatureShortcutGroup, R.string.keyboard_shortcut_history_manager,
                KeyEvent.KEYCODE_H, KeyEvent.META_CTRL_ON);
        addShortcut(context, chromeFeatureShortcutGroup, R.string.keyboard_shortcut_find_bar,
                KeyEvent.KEYCODE_F, KeyEvent.META_CTRL_ON);
        addShortcut(context, chromeFeatureShortcutGroup, R.string.keyboard_shortcut_address_bar,
                KeyEvent.KEYCODE_L, KeyEvent.META_CTRL_ON);
        shortcutGroups.add(chromeFeatureShortcutGroup);

        KeyboardShortcutGroup webpageShortcutGroup = new KeyboardShortcutGroup(
                context.getString(R.string.keyboard_shortcut_webpage_group_header));
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_print_page,
                KeyEvent.KEYCODE_P, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_reload_page,
                KeyEvent.KEYCODE_R, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_reload_no_cache,
                KeyEvent.KEYCODE_R, ctrlShift);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_bookmark_page,
                KeyEvent.KEYCODE_D, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_zoom_in,
                KeyEvent.KEYCODE_EQUALS, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_zoom_out,
                KeyEvent.KEYCODE_MINUS, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_reset_zoom,
                KeyEvent.KEYCODE_0, KeyEvent.META_CTRL_ON);
        addShortcut(context, webpageShortcutGroup, R.string.keyboard_shortcut_help_center,
                KeyEvent.KEYCODE_SLASH, ctrlShift);
        shortcutGroups.add(webpageShortcutGroup);

        return shortcutGroups;
    }

    @RequiresApi(Build.VERSION_CODES.N)
    private static void addShortcut(Context context, KeyboardShortcutGroup shortcutGroup, int resId,
            int keyCode, int keyModifier) {
        shortcutGroup.addItem(new KeyboardShortcutInfo(context.getString(resId), keyCode,
                keyModifier));
    }

}
