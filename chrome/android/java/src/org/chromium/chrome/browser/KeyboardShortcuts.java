// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.ui.KeyboardUtils.ALT;
import static org.chromium.base.ui.KeyboardUtils.CTRL;
import static org.chromium.base.ui.KeyboardUtils.NO_MODIFIER;
import static org.chromium.base.ui.KeyboardUtils.SHIFT;

import android.content.Context;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.KeyboardShortcutInfo;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.accessibility.AccessibilityState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;

/** Implements app-level keyboard shortcuts for ChromeTabbedActivity and DocumentActivity. */
public class KeyboardShortcuts {

    private static final LinkedHashMap<Integer, KeyboardShortcutDefinition>
            KEYBOARD_SHORTCUT_DEFINITION_MAP = new LinkedHashMap<>();
    private static final HashMap<Integer, Integer> KEYBOARD_SHORTCUT_SEMANTIC_MAP = new HashMap<>();

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
        KeyboardShortcutsSemanticMeaning.RELOAD_TAB_BYPASSING_CACHE,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB,
        KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TAB_SEARCH,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_MULTITASK_MENU,
        KeyboardShortcutsSemanticMeaning.CLOSE_TAB,
        KeyboardShortcutsSemanticMeaning.CLOSE_WINDOW,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_QUIT_CHROME,
        KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX,
        KeyboardShortcutsSemanticMeaning.JUMP_TO_SEARCH,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_DOWN,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_UP,
        KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_TOOLBAR,
        KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_BOOKMARKS,
        KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS,
        KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU,
        KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT,
        KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT,
        KeyboardShortcutsSemanticMeaning.TOGGLE_CARET_BROWSING,
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
        KeyboardShortcutsSemanticMeaning.VIEW_SOURCE,
        KeyboardShortcutsSemanticMeaning.TASK_MANAGER,
        KeyboardShortcutsSemanticMeaning.SAVE_PAGE,
        KeyboardShortcutsSemanticMeaning.SHOW_DOWNLOADS,
        KeyboardShortcutsSemanticMeaning.OPEN_HISTORY,
        KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK,
        KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD,
        KeyboardShortcutsSemanticMeaning.CLEAR_BROWSING_DATA,
        KeyboardShortcutsSemanticMeaning.PRINT,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BASIC_PRINT,
        KeyboardShortcutsSemanticMeaning.ZOOM_IN,
        KeyboardShortcutsSemanticMeaning.ZOOM_OUT,
        KeyboardShortcutsSemanticMeaning.ZOOM_RESET,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_AVATAR_MENU,
        KeyboardShortcutsSemanticMeaning.FEEDBACK_FORM,
        KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE,
        KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_HOME,
        KeyboardShortcutsSemanticMeaning.OPEN_HELP,
        KeyboardShortcutsSemanticMeaning.OPEN_MENU,
        KeyboardShortcutsSemanticMeaning.CUSTOM_EXTENSION_SHORTCUT,
        KeyboardShortcutsSemanticMeaning.TOGGLE_MULTISELECT,
        KeyboardShortcutsSemanticMeaning.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
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
        int RELOAD_TAB_BYPASSING_CACHE = 6;
        int MOVE_TO_TAB_LEFT = 7;
        int MOVE_TO_TAB_RIGHT = 8;
        int MOVE_TO_SPECIFIC_TAB = 9;
        int MOVE_TO_LAST_TAB = 10;
        int NOT_IMPLEMENTED_TAB_SEARCH = 11;
        int NOT_IMPLEMENTED_TOGGLE_MULTITASK_MENU = 12;

        // Closing.
        int CLOSE_TAB = 13;
        int CLOSE_WINDOW = 14;
        int NOT_IMPLEMENTED_QUIT_CHROME = 15;

        // Navigation controls.
        int JUMP_TO_OMNIBOX = 16;
        int JUMP_TO_SEARCH = 17;
        int NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE = 18;
        int NOT_IMPLEMENTED_SCROLL_DOWN = 19;
        int NOT_IMPLEMENTED_SCROLL_UP = 20;

        // Top controls.
        int KEYBOARD_FOCUS_TOOLBAR = 21;
        int KEYBOARD_FOCUS_BOOKMARKS = 22;
        int KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS = 23;
        int FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU = 24;
        int FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT = 25;
        int FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT = 26;
        int NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT = 27;
        int NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT = 28;

        // Accessibility.
        int TOGGLE_CARET_BROWSING = 29;
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
        int VIEW_SOURCE = 41;
        int TASK_MANAGER = 42;

        // Downloads.
        int SAVE_PAGE = 43;
        int SHOW_DOWNLOADS = 44;

        // History.
        int OPEN_HISTORY = 45;
        int HISTORY_GO_BACK = 46;
        int HISTORY_GO_FORWARD = 47;
        int CLEAR_BROWSING_DATA = 48;

        // Print.
        int PRINT = 49;
        int NOT_IMPLEMENTED_BASIC_PRINT = 50;

        // Zoom controls.
        int ZOOM_IN = 51;
        int ZOOM_OUT = 52;
        int ZOOM_RESET = 53;

        // Misc.
        int NOT_IMPLEMENTED_AVATAR_MENU = 54;
        int FEEDBACK_FORM = 55;
        int FIND_IN_PAGE = 56;
        int NOT_IMPLEMENTED_HOME = 57;
        int OPEN_HELP = 58;
        int OPEN_MENU = 59;

        // Custom keyboard shortcut used by extensions.
        // This enum isn't precisely a single semantic meaning, but we want to report metrics.
        int CUSTOM_EXTENSION_SHORTCUT = 60;

        // Tab strip shortcuts.
        int TOGGLE_MULTISELECT = 61;

        // Max value.
        int MAX_VALUE = 62;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:KeyboardShortcutsSemanticMeaning, //tools/metrics/histograms/metadata/accessibility/histograms.xml:KeyboardShortcutsSemanticMeaning)

    private static @KeyboardShortcutsSemanticMeaning int getKeyboardSemanticMeaning(
            KeyEvent event) {
        int keyCodeAndMeta = event.getKeyCode() | KeyboardUtils.getMetaState(event);
        if (KEYBOARD_SHORTCUT_SEMANTIC_MAP.containsKey(keyCodeAndMeta)) {
            return KEYBOARD_SHORTCUT_SEMANTIC_MAP.get(keyCodeAndMeta);
        }

        return KeyboardShortcutsSemanticMeaning.UNKNOWN;
    }

    /**
     * {@code KeyCombo} defines the structure for representing a single combination (keycode +
     * modifier) for a keyboard shortcut.
     */
    @NullMarked
    private static class KeyCombo {
        final int mKeyCode;
        final int mModifier;
        final int mMetaStateAndKeyCode;

        /**
         * Constructs a new {@link KeyCombo} object.
         *
         * @param keyCode An integer representing the key code of the primary key that triggers the
         *     shortcut (e.g. "KeyEvent.KEYCODE_K", "KeyEvent.KEYCODE_F5", etc.)
         * @param modifier An integer representing the modifier keys (e.g., Ctrl, Shift, Alt) held
         *     down in combination with the keycode.
         */
        private KeyCombo(int keyCode, int modifier) {
            mKeyCode = keyCode;
            mModifier = modifier;
            mMetaStateAndKeyCode = (getMetaState(modifier) | keyCode);
        }

        private int getMetaState(int modifier) {
            int metaState = 0;
            if ((modifier & KeyEvent.META_CTRL_ON) != 0) {
                metaState |= CTRL;
            }
            if ((modifier & KeyEvent.META_ALT_ON) != 0) {
                metaState |= ALT;
            }
            if ((modifier & KeyEvent.META_SHIFT_ON) != 0) {
                metaState |= SHIFT;
            }
            return metaState;
        }
    }

    /**
     * {@link KeyboardShortcutDefinition} defines the structure for representing information about a
     * single keyboard shortcut. This class is intended to hold details such as the trigger key
     * combination, the action performed by the shortcut, and potentially a docstring and category
     * for organization.
     *
     * <p>Instances of this class are typically used to store and manage the definitions of
     * available keyboard shortcuts for ChromeTabbedActivity and DocumentActivity.
     */
    @NullMarked
    private static class KeyboardShortcutDefinition {

        private final KeyCombo mPrimaryShortcut;
        private final @StringRes int mResId;
        private final @StringRes int mGroupId;

        /**
         * Constructs a new {@link KeyboardShortcutDefinition} object.
         *
         * @param semanticMeaning An integer representing the meaning or purpose of the shortcut.
         * @param primaryShortcut A KeyCombo object that contains the keycode and modifier for the
         *     shortcut. This shortcut will be added to the semantic map and the keyboard shortcut
         *     helper window.
         * @param resId An integer representing the @StringRes docstring for the shortcut.
         * @param groupId An integer representing the @StringRes docstring describing the shortcut
         *     group and associated header for the shortcut (e.g. "Developer shortcuts, Navigation
         *     shortcuts, etc.").
         * @param alternateShortcuts An array of KeyCombo objects that contain alternative keycode
         *     and modifier combinations for the shortcut. These will be added to the semantic map
         *     but will not be displayed in the keyboard shortcut helper window.
         */
        private KeyboardShortcutDefinition(
                @KeyboardShortcutsSemanticMeaning int semanticMeaning,
                KeyCombo primaryShortcut,
                @StringRes int resId,
                @StringRes int groupId,
                KeyCombo[] alternateShortcuts) {
            mPrimaryShortcut = primaryShortcut;
            mResId = resId;
            mGroupId = groupId;

            KEYBOARD_SHORTCUT_DEFINITION_MAP.put(primaryShortcut.mMetaStateAndKeyCode, this);
            KEYBOARD_SHORTCUT_SEMANTIC_MAP.put(
                    primaryShortcut.mMetaStateAndKeyCode, semanticMeaning);

            // Add alternate combinations to the semantic map, but not to the shortcut helper.
            for (var alternateShortcut : alternateShortcuts) {
                KEYBOARD_SHORTCUT_SEMANTIC_MAP.put(
                        alternateShortcut.mMetaStateAndKeyCode, semanticMeaning);
            }
        }

        /**
         * Build a new instance with no alternate key combinations.
         *
         * @param semanticMeaning An integer representing the meaning or purpose of the shortcut.
         * @param primaryShortcut A KeyCombo object that contains the keycode and modifier for the
         *     shortcut.
         * @param resId An integer representing the @StringRes docstring for the shortcut.
         * @param groupId An integer representing the shortcut group and associated header for the
         *     shortcut (e.g. "Developer shortcuts, Navigation shortcuts, etc.").
         */
        KeyboardShortcutDefinition(
                @KeyboardShortcutsSemanticMeaning int semanticMeaning,
                KeyCombo primaryShortcut,
                @StringRes int resId,
                @StringRes int groupId) {
            this(
                    semanticMeaning,
                    primaryShortcut,
                    resId,
                    groupId,
                    /* alternateShortcuts= */ new KeyCombo[] {});
        }

        /**
         * Build a new instance with no alternate key combinations and null integer values for resId
         * and groupId.
         *
         * @param semanticMeaning An integer representing the meaning or purpose of the shortcut.
         * @param primaryShortcut A KeyCombo object that contains the keycode and modifier for the
         *     shortcut.
         */
        KeyboardShortcutDefinition(
                @KeyboardShortcutsSemanticMeaning int semanticMeaning, KeyCombo primaryShortcut) {
            this(
                    semanticMeaning,
                    primaryShortcut,
                    /* resId= */ Resources.ID_NULL,
                    /* groupId= */ Resources.ID_NULL);
        }
    }

    // Adds all shortcuts to KEYBOARD_SHORTCUT_DEFINITION_MAP to be referenced by
    // createShortcutGroup() and KEYBOARD_SHORTCUT_SEMANTIC_MAP to be referenced by onKeyDown().
    static {
        // Tab control shortcuts (keyboard_shortcut_tab_group_header).
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_NEW_WINDOW,
                new KeyCombo(KeyEvent.KEYCODE_N, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_open_new_window,
                R.string.keyboard_shortcut_tab_group_header);
        // TODO(crbug.com/402775002): Change fn signature to allow (Alt + F then X) or
        // Command+Q
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.CLOSE_WINDOW,
                new KeyCombo(KeyEvent.KEYCODE_W, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_close_window,
                R.string.keyboard_shortcut_tab_group_header,
                new KeyCombo[] {new KeyCombo(KeyEvent.KEYCODE_F4, KeyEvent.META_ALT_ON)});

        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB,
                new KeyCombo(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_open_new_tab,
                R.string.keyboard_shortcut_tab_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_RECENTLY_CLOSED_TAB,
                new KeyCombo(KeyEvent.KEYCODE_T, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_reopen_new_tab,
                R.string.keyboard_shortcut_tab_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_NEW_TAB_INCOGNITO,
                new KeyCombo(KeyEvent.KEYCODE_N, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_new_incognito_tab,
                R.string.keyboard_shortcut_tab_group_header);

        // Reload tabs.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.RELOAD_TAB,
                new KeyCombo(KeyEvent.KEYCODE_R, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_reload_page,
                R.string.keyboard_shortcut_tab_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_F5, KeyEvent.META_SHIFT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_F5, NO_MODIFIER),
                    new KeyCombo(KeyEvent.KEYCODE_REFRESH, NO_MODIFIER)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.RELOAD_TAB_BYPASSING_CACHE,
                new KeyCombo(KeyEvent.KEYCODE_R, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_reload_no_cache,
                R.string.keyboard_shortcut_tab_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_F5, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_REFRESH, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_REFRESH, KeyEvent.META_SHIFT_ON)
                });

        // Close tabs.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.CLOSE_TAB,
                new KeyCombo(KeyEvent.KEYCODE_W, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_close_tab,
                R.string.keyboard_shortcut_tab_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_F4, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_B, NO_MODIFIER),
                });

        // Navigation shortcuts (keyboard_shortcut_tab_navigation_group_header).
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_RIGHT,
                new KeyCombo(KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_next_tab,
                R.string.keyboard_shortcut_tab_navigation_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_PAGE_DOWN, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_R1, NO_MODIFIER)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.MOVE_TO_TAB_LEFT,
                new KeyCombo(
                        KeyEvent.KEYCODE_TAB, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_prev_tab,
                R.string.keyboard_shortcut_tab_navigation_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_PAGE_UP, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_L1, NO_MODIFIER)
                });

        // Move to specific tab (1-8)
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.MOVE_TO_SPECIFIC_TAB,
                new KeyCombo(KeyEvent.KEYCODE_1, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_specific_tab,
                R.string.keyboard_shortcut_tab_navigation_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_2, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_3, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_4, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_5, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_6, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_7, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_8, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_1, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_2, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_3, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_4, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_5, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_6, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_7, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_8, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_1, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_2, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_3, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_4, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_5, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_6, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_7, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_8, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_1, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_2, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_3, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_4, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_5, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_6, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_7, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_8, KeyEvent.META_ALT_ON),
                });

        // Move to last tab.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.MOVE_TO_LAST_TAB,
                new KeyCombo(KeyEvent.KEYCODE_9, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_last_tab,
                R.string.keyboard_shortcut_tab_navigation_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_9, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_9, KeyEvent.META_CTRL_ON),
                    new KeyCombo(KeyEvent.KEYCODE_NUMPAD_9, KeyEvent.META_ALT_ON),
                });

        // Chrome feature shortcuts (keyboard_shortcut_chrome_feature_group_header).
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.JUMP_TO_OMNIBOX,
                new KeyCombo(KeyEvent.KEYCODE_L, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_address_bar,
                R.string.keyboard_shortcut_chrome_feature_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_D, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_X, NO_MODIFIER)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.JUMP_TO_SEARCH,
                new KeyCombo(KeyEvent.KEYCODE_E, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_search,
                R.string.keyboard_shortcut_chrome_feature_group_header,
                new KeyCombo[] {new KeyCombo(KeyEvent.KEYCODE_K, KeyEvent.META_CTRL_ON)});
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.FIND_IN_PAGE,
                new KeyCombo(KeyEvent.KEYCODE_F, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_find_bar,
                R.string.keyboard_shortcut_chrome_feature_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_G, KeyEvent.META_CTRL_ON),
                    new KeyCombo(
                            KeyEvent.KEYCODE_G, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                    new KeyCombo(KeyEvent.KEYCODE_F3, NO_MODIFIER),
                    new KeyCombo(KeyEvent.KEYCODE_F3, KeyEvent.META_SHIFT_ON)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_MENU,
                new KeyCombo(KeyEvent.KEYCODE_E, KeyEvent.META_ALT_ON),
                R.string.keyboard_shortcut_open_menu,
                R.string.keyboard_shortcut_chrome_feature_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_F, KeyEvent.META_ALT_ON),
                    new KeyCombo(KeyEvent.KEYCODE_F10, NO_MODIFIER),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_Y, NO_MODIFIER)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.FEEDBACK_FORM,
                new KeyCombo(KeyEvent.KEYCODE_I, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON),
                R.string.keyboard_shortcut_send_feedback,
                R.string.keyboard_shortcut_chrome_feature_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.SHOW_DOWNLOADS,
                new KeyCombo(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_show_downloads,
                R.string.keyboard_shortcut_chrome_feature_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.TOGGLE_CARET_BROWSING,
                new KeyCombo(KeyEvent.KEYCODE_F7, NO_MODIFIER),
                R.string.keyboard_shortcut_toggle_caret_browsing,
                R.string.keyboard_shortcut_chrome_feature_group_header);

        // History shortcuts
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_HISTORY,
                new KeyCombo(KeyEvent.KEYCODE_H, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_history_manager,
                R.string.keyboard_shortcut_chrome_feature_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK,
                new KeyCombo(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.META_ALT_ON),
                R.string.keyboard_shortcut_history_go_back,
                R.string.keyboard_shortcut_chrome_feature_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD,
                new KeyCombo(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.META_ALT_ON),
                R.string.keyboard_shortcut_history_go_forward,
                R.string.keyboard_shortcut_chrome_feature_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_FORWARD, NO_MODIFIER),
                    new KeyCombo(KeyEvent.KEYCODE_BUTTON_START, NO_MODIFIER)
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.CLEAR_BROWSING_DATA,
                new KeyCombo(
                        KeyEvent.KEYCODE_DEL, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_clear_browsing_data,
                R.string.keyboard_shortcut_chrome_feature_group_header);

        // Top controls.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_TOOLBAR,
                new KeyCombo(KeyEvent.KEYCODE_T, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_BOOKMARKS,
                new KeyCombo(KeyEvent.KEYCODE_B, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS,
                new KeyCombo(KeyEvent.KEYCODE_F6, NO_MODIFIER));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU,
                new KeyCombo(KeyEvent.KEYCODE_F10, KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT,
                new KeyCombo(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.META_CTRL_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT,
                new KeyCombo(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.META_CTRL_ON));

        // Bookmark shortcuts.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.TOGGLE_BOOKMARK_BAR,
                new KeyCombo(KeyEvent.KEYCODE_B, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON),
                /* resId= */ Resources.ID_NULL, // Purposefully setting to null
                // so that the shortcut is not shown in the helper unless the user has enabled it.
                /* groupId= */ Resources.ID_NULL);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.BOOKMARK_PAGE,
                new KeyCombo(KeyEvent.KEYCODE_D, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_bookmark_page,
                R.string.keyboard_shortcut_chrome_feature_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_BOOKMARKS,
                new KeyCombo(KeyEvent.KEYCODE_O, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                R.string.keyboard_shortcut_bookmark_manager,
                R.string.keyboard_shortcut_chrome_feature_group_header);

        // Developer tools.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.VIEW_SOURCE,
                new KeyCombo(KeyEvent.KEYCODE_U, KeyEvent.META_CTRL_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.DEV_TOOLS,
                new KeyCombo(KeyEvent.KEYCODE_I, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.TASK_MANAGER,
                new KeyCombo(KeyEvent.KEYCODE_ESCAPE, KeyEvent.META_CTRL_ON));

        // Webpage shortcuts (keyboard_shortcut_webpage_group_header).
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.PRINT,
                new KeyCombo(KeyEvent.KEYCODE_P, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_print_page,
                R.string.keyboard_shortcut_webpage_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.SAVE_PAGE,
                new KeyCombo(KeyEvent.KEYCODE_S, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_save_page,
                R.string.keyboard_shortcut_webpage_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.ZOOM_IN,
                new KeyCombo(KeyEvent.KEYCODE_PLUS, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_zoom_in,
                R.string.keyboard_shortcut_webpage_group_header,
                new KeyCombo[] {
                    new KeyCombo(KeyEvent.KEYCODE_ZOOM_IN, NO_MODIFIER),
                    new KeyCombo(KeyEvent.KEYCODE_EQUALS, KeyEvent.META_CTRL_ON),
                    new KeyCombo(
                            KeyEvent.KEYCODE_PLUS,
                            (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)),
                    new KeyCombo(
                            KeyEvent.KEYCODE_EQUALS,
                            (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON))
                });
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.ZOOM_OUT,
                new KeyCombo(KeyEvent.KEYCODE_MINUS, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_zoom_out,
                R.string.keyboard_shortcut_webpage_group_header,
                new KeyCombo[] {new KeyCombo(KeyEvent.KEYCODE_ZOOM_OUT, NO_MODIFIER)});
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.ZOOM_RESET,
                new KeyCombo(KeyEvent.KEYCODE_0, KeyEvent.META_CTRL_ON),
                R.string.keyboard_shortcut_reset_zoom,
                R.string.keyboard_shortcut_webpage_group_header);
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.OPEN_HELP,
                new KeyCombo(KeyEvent.KEYCODE_F1, NO_MODIFIER),
                R.string.keyboard_shortcut_help_center,
                R.string.keyboard_shortcut_webpage_group_header,
                new KeyCombo[] {
                    new KeyCombo(
                            KeyEvent.KEYCODE_SLASH,
                            (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON))
                });

        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.TOGGLE_MULTISELECT,
                new KeyCombo(KeyEvent.KEYCODE_H, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON),
                R.string.keyboard_shortcut_toggle_multiselect,
                R.string.keyboard_shortcut_tab_navigation_group_header);

        // Unimplemented shortcuts.
        // TODO(crbug.com/402775002): Figure out what shortcut does TOGGLE_MULTITASK_MENU.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TAB_SEARCH,
                new KeyCombo(KeyEvent.KEYCODE_A, (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON)));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_WEB_CONTENTS_PANE,
                new KeyCombo(KeyEvent.KEYCODE_F6, KeyEvent.META_CTRL_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_DOWN,
                new KeyCombo(KeyEvent.KEYCODE_SPACE, NO_MODIFIER));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_SCROLL_UP,
                new KeyCombo(KeyEvent.KEYCODE_SPACE, KeyEvent.META_SHIFT_ON));
        // TODO(crbug.com/402775002): Change fn signature to allow CTRL+SHIFT+FN+UpArrow.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_LEFT,
                new KeyCombo(
                        KeyEvent.KEYCODE_PAGE_UP, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        // TODO(crbug.com/402775002): Change fn signature to allow CTRL+SHIFT+FN+DownArrow.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_CURRENT_OPEN_TAB_REORDER_RIGHT,
                new KeyCombo(
                        KeyEvent.KEYCODE_PAGE_DOWN,
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_FOCUS_ON_INACTIVE_DIALOGS,
                new KeyCombo(KeyEvent.KEYCODE_A, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BOOKMARK_ALL_TABS,
                new KeyCombo(KeyEvent.KEYCODE_D, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        // TODO(crbug.com/402775002): Allow long press on Esc.
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_TOGGLE_IMMERSIVE,
                new KeyCombo(KeyEvent.KEYCODE_F11, NO_MODIFIER));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_CONSOLE,
                new KeyCombo(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_INSPECT,
                new KeyCombo(KeyEvent.KEYCODE_C, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_DEV_TOOLS_TOGGLE,
                new KeyCombo(KeyEvent.KEYCODE_F12, NO_MODIFIER));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_BASIC_PRINT,
                new KeyCombo(KeyEvent.KEYCODE_P, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_AVATAR_MENU,
                new KeyCombo(KeyEvent.KEYCODE_M, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        new KeyboardShortcutDefinition(
                KeyboardShortcutsSemanticMeaning.NOT_IMPLEMENTED_HOME,
                new KeyCombo(KeyEvent.KEYCODE_HOME, KeyEvent.META_ALT_ON));
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
            if (keyCode == KeyEvent.KEYCODE_SEARCH || keyCode == KeyEvent.KEYCODE_MENU) {
                return true;
            }
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
                if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() != 0) {
                    break;
                }

                if (KeyboardUtils.getMetaState(event) == 0) {
                    if (menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.esc_key, false)) {
                        return true;
                    }
                }

                // Exiting full screen takes priority over other actions when Escape is pressed,
                // regardless of modifier keys. This means for example that you cannot open the task
                // manager in full screen mode.
                // TODO(crbug.com/398061359): Remove when Esc key logic ships without a kill switch.
                if (!ChromeFeatureList.sEnableExclusiveAccessManager.isEnabled()) {
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
     * This method should be called when overriding from {@link
     * android.app.Activity#onProvideKeyboardShortcuts(List, android.view.Menu, int)} in an
     * activity. It will return a list of the possible shortcuts. If someone adds a shortcut they
     * also need to add an explanation in the appropriate group in this method so the user can see
     * it when this method is called.
     *
     * <p>Preventing inlining since this uses APIs only available on Android N, and this causes dex
     * validation failures on earlier versions if inlined.
     *
     * @param context We need an activity so we can call the strings from our resource.
     * @return a list of shortcuts organized into groups.
     */
    public static List<KeyboardShortcutGroup> createShortcutGroup(Context context) {
        LinkedHashMap<Integer, KeyboardShortcutGroup> shortcutGroupsById = new LinkedHashMap<>();
        for (KeyboardShortcutDefinition shortcutDefinition :
                KEYBOARD_SHORTCUT_DEFINITION_MAP.values()) {
            if (shortcutDefinition.mGroupId == Resources.ID_NULL) {
                continue;
            }
            addShortcut(
                    context,
                    shortcutGroupsById,
                    shortcutDefinition.mGroupId,
                    shortcutDefinition.mResId,
                    shortcutDefinition.mPrimaryShortcut.mKeyCode,
                    shortcutDefinition.mPrimaryShortcut.mModifier);
        }

        if (BookmarkBarUtils.isDeviceBookmarkBarCompatible(context)) {
            addShortcut(
                    context,
                    shortcutGroupsById,
                    R.string.keyboard_shortcut_chrome_feature_group_header,
                    R.string.keyboard_shortcut_toggle_bookmark_bar,
                    KeyEvent.KEYCODE_B,
                    (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        }
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)) {
            addShortcut(
                    context,
                    shortcutGroupsById,
                    R.string.keyboard_shortcut_developer_group_header,
                    R.string.keyboard_shortcut_view_source,
                    KeyEvent.KEYCODE_U,
                    KeyEvent.META_CTRL_ON);
            addShortcut(
                    context,
                    shortcutGroupsById,
                    R.string.keyboard_shortcut_developer_group_header,
                    R.string.keyboard_shortcut_developer_tools,
                    KeyEvent.KEYCODE_I,
                    (KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_CLANK)) {
            addShortcut(
                    context,
                    shortcutGroupsById,
                    R.string.keyboard_shortcut_developer_group_header,
                    R.string.keyboard_shortcut_task_manager,
                    KeyEvent.KEYCODE_ESCAPE,
                    KeyEvent.META_CTRL_ON);
        }
        return new ArrayList<>(shortcutGroupsById.values());
    }

    private static void addShortcut(
            Context context,
            LinkedHashMap<Integer, KeyboardShortcutGroup> shortcutGroupsById,
            int groupId,
            int resId,
            int keyCode,
            int keyModifier) {
        if (!shortcutGroupsById.containsKey(groupId)) {
            shortcutGroupsById.put(groupId, new KeyboardShortcutGroup(context.getString(groupId)));
        }
        KeyboardShortcutGroup shortcutGroup = shortcutGroupsById.get(groupId);
        shortcutGroup.addItem(
                new KeyboardShortcutInfo(context.getString(resId), keyCode, keyModifier));
    }

    private static void updateToolbarAfterReloading(
            ToolbarManager toolbarManager, Tab currentTab, WebContents currentWebContents) {
        if (toolbarManager != null
                && currentWebContents != null
                && currentWebContents.focusLocationBarByDefault()) {
            toolbarManager.revertLocationBarChanges();
        } else if (currentTab.getView() != null) {
            currentTab.getView().requestFocus();
        }
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
        if (event.getRepeatCount() != 0 || KeyEvent.isModifierKey(keyCode)) {
            return false;
        }
        if (KeyEvent.isGamepadButton(keyCode)) {
            if (GamepadList.isGamepadAPIActive()) {
                return false;
            }
        } else if (!event.isCtrlPressed()
                && !event.isAltPressed()
                && keyCode != KeyEvent.KEYCODE_F3
                && keyCode != KeyEvent.KEYCODE_F5
                && keyCode != KeyEvent.KEYCODE_F6
                && keyCode != KeyEvent.KEYCODE_F7
                && keyCode != KeyEvent.KEYCODE_F10
                && keyCode != KeyEvent.KEYCODE_FORWARD
                && keyCode != KeyEvent.KEYCODE_REFRESH) {
            return false;
        }

        TabModel currentTabModel = tabModelSelector.getCurrentModel();
        Tab currentTab = tabModelSelector.getCurrentTab();
        WebContents currentWebContents = currentTab == null ? null : currentTab.getWebContents();
        BrowserContextHandle browserContextHandle =
                currentTab == null ? null : currentTab.getProfile().getOriginalProfile();

        int tabCount = currentTabModel.getCount();

        @KeyboardShortcutsSemanticMeaning int semanticMeaning = getKeyboardSemanticMeaning(event);

        RecordHistogram.recordEnumeratedHistogram(
                AccessibilityState.isKnownScreenReaderEnabled()
                        ? "Accessibility.Android.KeyboardShortcut.ScreenReaderRunning3"
                        : "Accessibility.Android.KeyboardShortcut.NoScreenReader3",
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
            case KeyboardShortcutsSemanticMeaning.VIEW_SOURCE:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.view_source, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.DEV_TOOLS:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.dev_tools, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.TASK_MANAGER:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.task_manager, false);
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
            case KeyboardShortcutsSemanticMeaning.FEEDBACK_FORM:
                menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.feedback_form, false);
                return true;
            case KeyboardShortcutsSemanticMeaning.TOGGLE_BOOKMARK_BAR:
                return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        R.id.toggle_bookmark_bar, /* fromMenu= */ false);
            case KeyboardShortcutsSemanticMeaning.CLOSE_WINDOW:
                return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                        R.id.close_window, /* fromMenu= */ false);
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
                    List<Tab> selectedTabs = new ArrayList<>();
                    for (int i = 0; i < currentTabModel.getCount(); i++) {
                        @Nullable Tab tab = currentTabModel.getTabAt(i);
                        if (tab == null) continue;
                        if (!currentTabModel.isTabMultiSelected(tab.getId())) continue;
                        selectedTabs.add(tab);
                    }
                    Tab tab = TabModelUtils.getCurrentTab(currentTabModel);
                    if (tab != null) {
                        currentTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTabs(selectedTabs)
                                                .allowUndo(false)
                                                .tabClosingSource(
                                                        TabClosingSource.KEYBOARD_SHORTCUT)
                                                .build(),
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
                case KeyboardShortcutsSemanticMeaning.SHOW_DOWNLOADS:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.downloads_menu_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.CLEAR_BROWSING_DATA:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.quick_delete_menu_id, false);
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
                    ZoomController.zoomReset(currentWebContents, browserContextHandle);
                    return true;
                case KeyboardShortcutsSemanticMeaning.RELOAD_TAB:
                    if (currentTab != null) {
                        currentTab.reload();
                        updateToolbarAfterReloading(toolbarManager, currentTab, currentWebContents);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.RELOAD_TAB_BYPASSING_CACHE:
                    if (currentTab != null) {
                        currentTab.reloadIgnoringCache();
                        updateToolbarAfterReloading(toolbarManager, currentTab, currentWebContents);
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.HISTORY_GO_BACK:
                    if (currentTab != null && currentTab.canGoBack()) {
                        currentTab.goBack();
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.HISTORY_GO_FORWARD:
                    if (currentTab != null && currentTab.canGoForward()) {
                        currentTab.goForward();
                    }
                    return true;
                case KeyboardShortcutsSemanticMeaning.TOGGLE_CARET_BROWSING:
                    if (ContentFeatureList.sAndroidCaretBrowsing.isEnabled()) {
                        menuOrKeyboardActionController.onMenuOrKeyboardAction(
                                R.id.toggle_caret_browsing, false);
                        return true;
                    }
                    return false;
                case KeyboardShortcutsSemanticMeaning.OPEN_HELP:
                    menuOrKeyboardActionController.onMenuOrKeyboardAction(R.id.help_id, false);
                    return true;
                case KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_SWITCH_ROW_OF_TOP_ELEMENTS:
                    // TODO(crbug.com/360423850): Don't allow F6 to be overridden by websites.
                    return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.switch_keyboard_focus_row, /* fromMenu= */ false);
                case KeyboardShortcutsSemanticMeaning
                        .FOCUSED_TAB_STRIP_ITEM_OPEN_CONTEXT_MENU:
                    return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.open_tab_strip_context_menu, /* fromMenu= */ false);
                case KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_LEFT:
                    return toolbarManager.reorderKeyboardFocusedItem(/* toLeft= */ true);
                case KeyboardShortcutsSemanticMeaning.FOCUSED_TAB_STRIP_ITEM_REORDER_RIGHT:
                    return toolbarManager.reorderKeyboardFocusedItem(/* toLeft= */ false);
                case KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_TOOLBAR:
                    toolbarManager.requestFocus();
                    return true;
                case KeyboardShortcutsSemanticMeaning.KEYBOARD_FOCUS_BOOKMARKS:
                    return menuOrKeyboardActionController.onMenuOrKeyboardAction(
                            R.id.focus_bookmarks, /* fromMenu= */ false);
                case KeyboardShortcutsSemanticMeaning.TOGGLE_MULTISELECT:
                    return toolbarManager.multiselectKeyboardFocusedItem();
            }
        }

        return false;
    }

    @CalledByNative
    private static boolean isChromeAccelerator(KeyEvent event) {
        return getKeyboardSemanticMeaning(event) != KeyboardShortcutsSemanticMeaning.UNKNOWN;
    }
}
