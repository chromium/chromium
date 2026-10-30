// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shortcuts;

import android.content.res.Resources;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeCommandIds;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Evaluates command enablement and executes browser commands (defined in {@link ChromeCommandIds})
 * by routing them through {@link MenuOrKeyboardActionController}.
 */
@NullMarked
public class BrowserCommandDelegate {
    private final BooleanSupplier mIsUiInitializedSupplier;
    private final BooleanSupplier mIsCurrentTabVisibleSupplier;
    private final boolean mTabbedBrowserCommandsSupported;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    /**
     * Creates a {@link BrowserCommandDelegate} for a browser window.
     *
     * @param isUiInitializedSupplier Supplies whether the window's UI is initialized.
     * @param isCurrentTabVisibleSupplier Supplies whether the active tab is currently visible (not
     *     in the tab switcher / Hub).
     * @param tabbedBrowserCommandsSupported Whether multi-tab window commands are supported (e.g.
     *     true for ChromeTabbedActivity). Commands operating on the active tab itself (e.g. FIND)
     *     are supported regardless.
     * @param tabModelSelectorSupplier Supplies the window's {@link TabModelSelector}.
     * @param menuOrKeyboardActionController Controller that executes {@code R.id.*} actions.
     */
    public BrowserCommandDelegate(
            BooleanSupplier isUiInitializedSupplier,
            BooleanSupplier isCurrentTabVisibleSupplier,
            boolean tabbedBrowserCommandsSupported,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
        mIsUiInitializedSupplier = isUiInitializedSupplier;
        mIsCurrentTabVisibleSupplier = isCurrentTabVisibleSupplier;
        mTabbedBrowserCommandsSupported = tabbedBrowserCommandsSupported;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
    }

    /**
     * Executes the given browser command if enabled.
     *
     * @param commandId The {@link ChromeCommandIds} constant to execute.
     * @return {@code true} if the command was handled.
     */
    @CalledByNative
    public boolean executeCommand(int commandId) {
        if (!isCommandEnabled(commandId)) {
            return false;
        }

        int actionId = getActionIdForCommand(commandId);
        if (actionId == Resources.ID_NULL) {
            return false;
        }

        return mMenuOrKeyboardActionController.onMenuOrKeyboardAction(
                actionId, /* fromMenu= */ false);
    }

    /**
     * Returns whether the given browser command is currently enabled in this window.
     *
     * @param commandId The {@link ChromeCommandIds} constant to check.
     * @return {@code true} if the command is enabled and can be executed.
     */
    boolean isCommandEnabled(int commandId) {
        if (!mIsUiInitializedSupplier.getAsBoolean()) {
            return false;
        }

        boolean isCurrentTabVisible = mIsCurrentTabVisibleSupplier.getAsBoolean();

        // LINT.IfChange(BrowserCommandDelegateCommands)
        switch (commandId) {
            case ChromeCommandIds.EXIT:
            case ChromeCommandIds.CLOSE_FIND_OR_STOP:
            case ChromeCommandIds.SHOW_APP_MENU:
            case ChromeCommandIds.CLOSE_WINDOW:
            case ChromeCommandIds.TASK_MANAGER_SHORTCUT:
                return true;

            case ChromeCommandIds.FEEDBACK:
                return FeedbackPolicyManager.getInstance().isUserFeedbackAllowed();

            case ChromeCommandIds.NEW_INCOGNITO_WINDOW:
                return mTabbedBrowserCommandsSupported
                        && (getCurrentProfile() != null
                                && IncognitoUtils.isIncognitoModeEnabled(getCurrentProfile()));

            case ChromeCommandIds.NEW_TAB:
            case ChromeCommandIds.SELECT_NEXT_TAB:
            case ChromeCommandIds.SELECT_PREVIOUS_TAB:
            case ChromeCommandIds.CYCLE_TO_NEXT_TAB:
            case ChromeCommandIds.CYCLE_TO_PREV_TAB:
            case ChromeCommandIds.RESTORE_TAB:
            case ChromeCommandIds.FOCUS_NEXT_PANE:
            case ChromeCommandIds.FOCUS_PREVIOUS_PANE:
            case ChromeCommandIds.SHOW_BOOKMARK_BAR:
                return mTabbedBrowserCommandsSupported;

            case ChromeCommandIds.NEW_WINDOW:
                return mTabbedBrowserCommandsSupported
                        && MultiWindowUtils.isMultiInstanceApi31Enabled();

            case ChromeCommandIds.SELECT_TAB_0:
            case ChromeCommandIds.SELECT_TAB_1:
            case ChromeCommandIds.SELECT_TAB_2:
            case ChromeCommandIds.SELECT_TAB_3:
            case ChromeCommandIds.SELECT_TAB_4:
            case ChromeCommandIds.SELECT_TAB_5:
            case ChromeCommandIds.SELECT_TAB_6:
            case ChromeCommandIds.SELECT_TAB_7:
            case ChromeCommandIds.SELECT_LAST_TAB:
            case ChromeCommandIds.TOGGLE_MULTISELECT:
            case ChromeCommandIds.OPEN_TAB_STRIP_CONTEXT_MENU:
            case ChromeCommandIds.REORDER_TAB_STRIP_ITEM_LEFT:
            case ChromeCommandIds.REORDER_TAB_STRIP_ITEM_RIGHT:
            case ChromeCommandIds.BOOKMARK_ALL_TABS:
            case ChromeCommandIds.FOCUS_LOCATION:
            case ChromeCommandIds.FOCUS_SEARCH:
            case ChromeCommandIds.FOCUS_BOOKMARKS:
            case ChromeCommandIds.SHOW_BOOKMARK_MANAGER:
            case ChromeCommandIds.SHOW_DOWNLOADS:
            case ChromeCommandIds.CLEAR_BROWSING_DATA:
            case ChromeCommandIds.HELP_PAGE_VIA_KEYBOARD:
            case ChromeCommandIds.CARET_BROWSING_TOGGLE:
            case ChromeCommandIds.TAB_SEARCH:
                return isCurrentTabVisible && mTabbedBrowserCommandsSupported;

            case ChromeCommandIds.OPEN_FILE:
                return isCurrentTabVisible
                        && mTabbedBrowserCommandsSupported
                        && ChromeFeatureList.sAndroidKeyboardShortcutOpenFile.isEnabled();

            case ChromeCommandIds.BACK:
            case ChromeCommandIds.FORWARD:
            case ChromeCommandIds.RELOAD:
            case ChromeCommandIds.RELOAD_BYPASSING_CACHE:
            case ChromeCommandIds.CLOSE_TAB:
            case ChromeCommandIds.BOOKMARK_THIS_TAB:
            case ChromeCommandIds.HOME:
            case ChromeCommandIds.VIEW_SOURCE:
            case ChromeCommandIds.PRINT:
            case ChromeCommandIds.SAVE_PAGE:
            case ChromeCommandIds.FIND:
            case ChromeCommandIds.FIND_NEXT:
            case ChromeCommandIds.FIND_PREVIOUS:
            case ChromeCommandIds.ZOOM_PLUS:
            case ChromeCommandIds.ZOOM_NORMAL:
            case ChromeCommandIds.ZOOM_MINUS:
            case ChromeCommandIds.FOCUS_TOOLBAR:
            case ChromeCommandIds.FOCUS_MENU_BAR:
            case ChromeCommandIds.DEV_TOOLS:
            case ChromeCommandIds.DEV_TOOLS_CONSOLE:
            case ChromeCommandIds.DEV_TOOLS_INSPECT:
            case ChromeCommandIds.SHOW_HISTORY:
                return isCurrentTabVisible;

            default:
                return false;
        }
        // LINT.ThenChange(:BrowserCommandDelegateActions,//chrome/android/java/src/org/chromium/chrome/browser/KeyboardShortcuts.java:KeyboardShortcutsOnKeyDown)
    }

    private int getActionIdForCommand(int commandId) {
        // LINT.IfChange(BrowserCommandDelegateActions)
        switch (commandId) {
            case ChromeCommandIds.BACK:
                return R.id.back_menu_id;
            case ChromeCommandIds.FORWARD:
                return R.id.forward_menu_id;
            case ChromeCommandIds.RELOAD:
                return R.id.reload_menu_id;
            case ChromeCommandIds.HOME:
                return R.id.open_home_page;
            case ChromeCommandIds.RELOAD_BYPASSING_CACHE:
                return R.id.reload_ignoring_cache;
            case ChromeCommandIds.NEW_WINDOW:
                return (getCurrentProfile() != null
                                && IncognitoUtils.isIncognitoModeForced(getCurrentProfile()))
                        ? R.id.new_incognito_window_menu_id
                        : R.id.new_window_menu_id;
            case ChromeCommandIds.NEW_INCOGNITO_WINDOW:
                return IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? R.id.new_incognito_window_menu_id
                        : R.id.new_incognito_tab_menu_id;
            case ChromeCommandIds.CLOSE_WINDOW:
                return R.id.close_window;
            case ChromeCommandIds.NEW_TAB:
                {
                    TabModel currentTabModel = getCurrentTabModel();
                    boolean openIncognito =
                            (getCurrentProfile() != null
                                            && IncognitoUtils.isIncognitoModeForced(
                                                    getCurrentProfile()))
                                    || (currentTabModel != null && currentTabModel.isIncognito());
                    return openIncognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id;
                }
            case ChromeCommandIds.CLOSE_TAB:
                return R.id.close_tab;
            case ChromeCommandIds.SELECT_NEXT_TAB:
            case ChromeCommandIds.CYCLE_TO_NEXT_TAB:
                return R.id.select_next_tab;
            case ChromeCommandIds.SELECT_PREVIOUS_TAB:
            case ChromeCommandIds.CYCLE_TO_PREV_TAB:
                return R.id.select_previous_tab;
            case ChromeCommandIds.SELECT_TAB_0:
                return R.id.select_tab_0;
            case ChromeCommandIds.SELECT_TAB_1:
                return R.id.select_tab_1;
            case ChromeCommandIds.SELECT_TAB_2:
                return R.id.select_tab_2;
            case ChromeCommandIds.SELECT_TAB_3:
                return R.id.select_tab_3;
            case ChromeCommandIds.SELECT_TAB_4:
                return R.id.select_tab_4;
            case ChromeCommandIds.SELECT_TAB_5:
                return R.id.select_tab_5;
            case ChromeCommandIds.SELECT_TAB_6:
                return R.id.select_tab_6;
            case ChromeCommandIds.SELECT_TAB_7:
                return R.id.select_tab_7;
            case ChromeCommandIds.SELECT_LAST_TAB:
                return R.id.select_last_tab;
            case ChromeCommandIds.RESTORE_TAB:
                return R.id.open_recently_closed_tab;
            case ChromeCommandIds.EXIT:
                return R.id.quit_chrome;
            case ChromeCommandIds.CLOSE_FIND_OR_STOP:
                return R.id.esc_key;
            case ChromeCommandIds.TOGGLE_MULTISELECT:
                return R.id.toggle_tab_strip_multiselect;
            case ChromeCommandIds.OPEN_TAB_STRIP_CONTEXT_MENU:
                return R.id.open_tab_strip_context_menu;
            case ChromeCommandIds.REORDER_TAB_STRIP_ITEM_LEFT:
                return R.id.reorder_tab_strip_item_left;
            case ChromeCommandIds.REORDER_TAB_STRIP_ITEM_RIGHT:
                return R.id.reorder_tab_strip_item_right;
            case ChromeCommandIds.BOOKMARK_THIS_TAB:
                return R.id.bookmark_this_page_id;
            case ChromeCommandIds.BOOKMARK_ALL_TABS:
                return R.id.bookmark_all_tabs;
            case ChromeCommandIds.VIEW_SOURCE:
                return R.id.view_source;
            case ChromeCommandIds.PRINT:
                return R.id.print_id;
            case ChromeCommandIds.SAVE_PAGE:
                return R.id.offline_page_id;
            case ChromeCommandIds.FIND:
            case ChromeCommandIds.FIND_NEXT:
            case ChromeCommandIds.FIND_PREVIOUS:
                return R.id.find_in_page_id;
            case ChromeCommandIds.ZOOM_PLUS:
                return R.id.zoom_in;
            case ChromeCommandIds.ZOOM_NORMAL:
                return R.id.zoom_reset;
            case ChromeCommandIds.ZOOM_MINUS:
                return R.id.zoom_out;
            case ChromeCommandIds.FOCUS_TOOLBAR:
                return R.id.focus_toolbar;
            case ChromeCommandIds.FOCUS_LOCATION:
                return R.id.focus_url_bar;
            case ChromeCommandIds.FOCUS_SEARCH:
                return R.id.focus_and_clear_url_bar;
            case ChromeCommandIds.FOCUS_MENU_BAR:
                return R.id.focus_app_menu_button;
            case ChromeCommandIds.FOCUS_NEXT_PANE:
                return R.id.switch_keyboard_focus_row;
            case ChromeCommandIds.FOCUS_PREVIOUS_PANE:
                return R.id.switch_keyboard_focus_row_reverse;
            case ChromeCommandIds.FOCUS_BOOKMARKS:
                return R.id.focus_bookmarks;
            case ChromeCommandIds.OPEN_FILE:
                return R.id.open_file_id;
            case ChromeCommandIds.DEV_TOOLS:
                return R.id.dev_tools;
            case ChromeCommandIds.DEV_TOOLS_CONSOLE:
                return R.id.dev_tools_console;
            case ChromeCommandIds.FEEDBACK:
                return R.id.feedback_form;
            case ChromeCommandIds.SHOW_BOOKMARK_BAR:
                return R.id.toggle_bookmark_bar;
            case ChromeCommandIds.SHOW_HISTORY:
                return R.id.open_history_menu_id;
            case ChromeCommandIds.SHOW_BOOKMARK_MANAGER:
                return R.id.all_bookmarks_menu_id;
            case ChromeCommandIds.SHOW_DOWNLOADS:
                return R.id.downloads_menu_id;
            case ChromeCommandIds.CLEAR_BROWSING_DATA:
                return R.id.quick_delete_menu_id;
            case ChromeCommandIds.HELP_PAGE_VIA_KEYBOARD:
                return R.id.help_id;
            case ChromeCommandIds.SHOW_APP_MENU:
                return R.id.show_menu;
            case ChromeCommandIds.DEV_TOOLS_INSPECT:
                return R.id.dev_tools_inspect;
            case ChromeCommandIds.CARET_BROWSING_TOGGLE:
                return R.id.toggle_caret_browsing;
            case ChromeCommandIds.TASK_MANAGER_SHORTCUT:
                return R.id.task_manager;
            case ChromeCommandIds.TAB_SEARCH:
                return ChromeFeatureList.sTabSearchForDesktop.isEnabled()
                        ? R.id.tab_search_side_ui
                        : R.id.tab_search;
            default:
                return Resources.ID_NULL;
        }
        // LINT.ThenChange(:BrowserCommandDelegateCommands,//chrome/android/java/src/org/chromium/chrome/browser/KeyboardShortcuts.java:KeyboardShortcutsOnKeyDown)
    }

    private @Nullable Profile getCurrentProfile() {
        TabModel tabModel = getCurrentTabModel();
        return tabModel != null ? tabModel.getProfile() : null;
    }

    private @Nullable TabModel getCurrentTabModel() {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        return selector != null ? selector.getCurrentModel() : null;
    }
}
