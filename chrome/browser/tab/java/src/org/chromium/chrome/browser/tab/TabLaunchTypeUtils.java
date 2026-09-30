// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.PageTransition;

/**
 * Utility methods for querying behavioral traits and predicates of {@link TabLaunchType}.
 *
 * <p>TODO(crbug.com/543021442): Audit these predicates to see if any legacy caller behaviors should
 * be consolidated or cleaned up.
 */
@NullMarked
public final class TabLaunchTypeUtils {
    private TabLaunchTypeUtils() {}

    private static void assertValidLaunchType(@TabLaunchType int type) {
        assert type >= 0 && type < TabLaunchType.SIZE : "Invalid TabLaunchType: " + type;
    }

    /**
     * Returns true if the given launch type creates a tab in the background without immediately
     * stealing user focus or activating the tab.
     *
     * @param type The launch type to inspect.
     * @return True if the tab is launched in the background.
     */
    public static boolean isBackgroundLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_RECENT_TABS,
                    TabLaunchType.FROM_SYNC_BACKGROUND,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND,
                    TabLaunchType.FROM_OMNIBOX_BACKGROUND ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the given launch type corresponds to tab restoration where the active tab
     * selection and indexing is handled externally by session or state restore machinery.
     *
     * <p>Callers (such as {@code TabModelOrderController#willOpenInForeground}) rely on this
     * predicate to ensure that tabs created during restore flows are not automatically selected or
     * treated as newly opened user tabs.
     *
     * <ul>
     *   <li>{@link TabLaunchType#FROM_RESTORE}: Standard session or tab state restoration.
     *   <li>{@link TabLaunchType#FROM_RESTORE_TABS_UI}: Explicit user-initiated tab restore from
     *       UI.
     *   <li>{@link TabLaunchType#FROM_BROWSER_ACTIONS}: External browser action restore where
     *       active tab selection and indexing are managed externally by the caller.
     * </ul>
     *
     * @param type The launch type to inspect.
     * @return True if the launch is a restoration launch.
     */
    public static boolean isRestoreLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabLaunchType.FROM_RESTORE_TABS_UI ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the given launch type explicitly specifies that the tab should belong to a
     * tab group.
     *
     * @param type The launch type to inspect.
     * @return True if the tab should be launched into a tab group.
     */
    public static boolean shouldLaunchAsGroupedTab(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_TAB_GROUP_UI,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type requires calculating adjacency relative to an opener or
     * parent tab rather than appending to the end of the tab model.
     *
     * @param type The launch type to inspect.
     * @return True if the new tab should be placed adjacent to an opener or parent tab.
     */
    public static boolean shouldOpenAdjacent(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LINK,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type represents a tab being reparented into a new window or
     * activity (either in the foreground or background).
     *
     * @param type The launch type to inspect.
     * @return True if the tab is being reparented into a window.
     */
    public static boolean isReparentingLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_REPARENTING, TabLaunchType.FROM_REPARENTING_BACKGROUND -> true;
            default -> false;
        };
    }

    /**
     * Returns true if a newly added tab will be selected as the active tab by the layout manager.
     *
     * @param type The launch type of the added tab.
     * @param isTabIncognito Whether the added tab is incognito.
     * @param isIncognitoSelected Whether the incognito tab model is currently selected.
     * @return True if the added tab will be selected.
     */
    public static boolean willAddedTabBeSelected(
            @TabLaunchType int type, boolean isTabIncognito, boolean isIncognitoSelected) {
        assertValidLaunchType(type);
        if (type == TabLaunchType.FROM_RESTORE) return false;
        boolean isBackgroundLaunch = isBackgroundLaunch(type) || isRestoreLaunch(type);
        return !isBackgroundLaunch || (!isIncognitoSelected && isTabIncognito);
    }

    /**
     * Returns true if the layout manager should skip the tab-creating animation for a tab launched
     * with the given type.
     *
     * @param type The launch type to inspect.
     * @return True if the tab-creating animation should be skipped.
     */
    public static boolean shouldSkipTabCreatingAnimation(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_REPARENTING,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_EXTERNAL_APP,
                    TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                    TabLaunchType.FROM_STARTUP,
                    TabLaunchType.FROM_APP_WIDGET,
                    TabLaunchType.FROM_SYNC_BACKGROUND ->
                    true;
            default -> false;
        };
    }

    /**
     * Converts a {@link TabLaunchType} to a histogram key used in the {@code
     * Android.Tab.CreateNewTabDuration.{TabLaunchType}} histogram. These must be kept in sync.
     *
     * @param tabLaunchType The tab launch type.
     * @return The histogram suffix string.
     */
    public static String tabLaunchTypeToHistogramKey(@TabLaunchType int tabLaunchType) {
        assertValidLaunchType(tabLaunchType);
        return switch (tabLaunchType) {
            case TabLaunchType.FROM_LINK -> "Link";
            case TabLaunchType.FROM_EXTERNAL_APP -> "ExternalApp";
            case TabLaunchType.FROM_CHROME_UI -> "ChromeUI";
            case TabLaunchType.FROM_RESTORE -> "Restore";
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND ->
                    "LongressForeground"; // Preserved typo for UMA continuity
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND -> "LongpressBackground";
            case TabLaunchType.FROM_REPARENTING -> "Reparenting";
            case TabLaunchType.FROM_LAUNCHER_SHORTCUT -> "LauncherShortcut";
            case TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION ->
                    "SpeculativeBackgroundCreation";
            case TabLaunchType.FROM_BROWSER_ACTIONS -> "BrowserActions";
            case TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB -> "NewIncognitoTab";
            case TabLaunchType.FROM_STARTUP -> "Startup";
            case TabLaunchType.FROM_SESSION_STARTUP_WITH_URLS_PREF -> "SessionStartupWithUrlsPref";
            case TabLaunchType.FROM_START_SURFACE -> "StartSurface";
            case TabLaunchType.FROM_TAB_GROUP_UI -> "TabGroupUI";
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP -> "LongpressBackgroundInGroup";
            case TabLaunchType.FROM_APP_WIDGET -> "AppWidget";
            case TabLaunchType.FROM_LONGPRESS_INCOGNITO -> "LongpressIncognito";
            case TabLaunchType.FROM_RECENT_TABS -> "RecentTabs";
            case TabLaunchType.FROM_READING_LIST -> "ReadingList";
            case TabLaunchType.FROM_TAB_SWITCHER_UI -> "TabSwitcherUI";
            case TabLaunchType.FROM_RESTORE_TABS_UI -> "RestoreTabsUI";
            case TabLaunchType.FROM_OMNIBOX -> "Omnibox";
            case TabLaunchType.FROM_OMNIBOX_BACKGROUND -> "OmniboxBackground";
            case TabLaunchType.UNSET -> "Unset";
            case TabLaunchType.FROM_SYNC_BACKGROUND -> "SyncBackground";
            case TabLaunchType.FROM_RECENT_TABS_FOREGROUND -> "RecentTabsForeground";
            case TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP ->
                    "CollaborationBackgroundInGroup";
            case TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND -> "BookmarkBarBackground";
            case TabLaunchType.FROM_REPARENTING_BACKGROUND -> "ReparentingBackground";
            case TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND -> "HistoryNavigationBackground";
            case TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND ->
                    "HistoryNavigationBackground"; // Preserved legacy alias
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP -> "LongpressForegroundInGroup";
            case TabLaunchType.FROM_TAB_LIST_INTERFACE -> "TabListInterface";
            case TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW -> "LinkToNewWindow";
            case TabLaunchType.FROM_TIPS_NOTIFICATIONS -> "TipsNotifications";
            case TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND -> "TabListInterfaceBackground";
            default -> {
                assert false : "Unexpected serialization of tabLaunchType: " + tabLaunchType;
                yield "TypeUnknown";
            }
        };
    }

    /**
     * Returns the default {@link PageTransition} for a tab launched with the given {@link
     * TabLaunchType}, prior to any intent-specific transition overrides.
     *
     * @param type The {@link TabLaunchType} with which the tab is launched.
     * @param originalTransitionType The initial transition type from {@code LoadUrlParams}.
     * @return The resolved {@link PageTransition} bitmask.
     */
    @SuppressWarnings("WrongConstant")
    @PageTransition
    public static int getDefaultPageTransition(
            @TabLaunchType int type, int originalTransitionType) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_OMNIBOX, TabLaunchType.FROM_OMNIBOX_BACKGROUND ->
                    originalTransitionType;
            case TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_LINK,
                    TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW,
                    TabLaunchType.FROM_EXTERNAL_APP,
                    TabLaunchType.FROM_BROWSER_ACTIONS ->
                    // FROM_API ensures intent handling isn't used.
                    PageTransition.LINK | PageTransition.FROM_API;
            case TabLaunchType.FROM_CHROME_UI,
                    TabLaunchType.FROM_TAB_SWITCHER_UI,
                    TabLaunchType.FROM_RESTORE_TABS_UI,
                    TabLaunchType.FROM_TAB_GROUP_UI,
                    TabLaunchType.FROM_STARTUP,
                    TabLaunchType.FROM_SESSION_STARTUP_WITH_URLS_PREF,
                    TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                    TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                    TabLaunchType.FROM_APP_WIDGET,
                    TabLaunchType.FROM_READING_LIST,
                    TabLaunchType.FROM_SYNC_BACKGROUND,
                    TabLaunchType.FROM_REPARENTING,
                    TabLaunchType.FROM_START_SURFACE ->
                    PageTransition.AUTO_TOPLEVEL;
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND ->
                    PageTransition.LINK;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_RECENT_TABS,
                    TabLaunchType.FROM_RECENT_TABS_FOREGROUND,
                    TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION,
                    TabLaunchType.FROM_TAB_LIST_INTERFACE,
                    TabLaunchType.FROM_TIPS_NOTIFICATIONS,
                    TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND ->
                    // On low end devices tabs are backgrounded in a frozen state, so we set the
                    // transition type to RELOAD to avoid handling intents when the tab is
                    // foregrounded. (https://crbug.com/40536523)
                    SysUtils.isLowEndDevice() ? PageTransition.RELOAD : PageTransition.LINK;
            case TabLaunchType.UNSET -> {
                assert false : "Unexpected TabLaunchType.UNSET in getDefaultPageTransition";
                yield PageTransition.LINK;
            }
            default -> {
                assert false : "Unexpected TabLaunchType: " + type;
                yield PageTransition.LINK;
            }
        };
    }

    /**
     * Returns true if the launch type represents a tab opened in the foreground from the longpress
     * context menu (either ungrouped or within a tab group).
     *
     * @param type The launch type to inspect.
     * @return True if the tab is launched in the foreground from a longpress context menu.
     */
    public static boolean isLongpressForegroundLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type represents a tab opened in the background from the longpress
     * context menu (either ungrouped or within a tab group).
     *
     * @param type The launch type to inspect.
     * @return True if the tab is launched in the background from a longpress context menu.
     */
    public static boolean isLongpressBackgroundLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the new tab animation layout should keep the current tab selected without
     * switching to the newly created tab.
     *
     * @param type The launch type to inspect.
     * @return True if the current tab should remain selected during animation.
     */
    public static boolean shouldKeepCurrentTabOnAnimation(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_TIPS_NOTIFICATIONS ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if adding a tab with the given launch type should trigger the "Opened in new
     * tab" toast notification.
     *
     * @param type The launch type to inspect.
     * @param animationsEnabled Whether device-class animations are enabled.
     * @return True if the open-in-new-tab toast should be shown.
     */
    public static boolean shouldShowOpenInNewTabToast(
            @TabLaunchType int type, boolean animationsEnabled) {
        assertValidLaunchType(type);
        return isLongpressBackgroundLaunch(type)
                || (type == TabLaunchType.FROM_RECENT_TABS && !animationsEnabled);
    }

    /**
     * Returns true if pressing the back button on a tab with no navigation history should close the
     * tab rather than sending the activity to the background.
     *
     * @param tab The tab to inspect.
     * @return True if pressing back should close the tab.
     */
    public static boolean shouldCloseTabOnBackPress(Tab tab) {
        @TabLaunchType int type = tab.getLaunchType();
        assertValidLaunchType(type);
        boolean hasParent = tab.getParentId() != Tab.INVALID_TAB_ID;
        return switch (type) {
            case TabLaunchType.FROM_LINK,
                    TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW,
                    TabLaunchType.FROM_EXTERNAL_APP,
                    TabLaunchType.FROM_READING_LIST,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_RECENT_TABS,
                    TabLaunchType.FROM_RECENT_TABS_FOREGROUND ->
                    true;
            case TabLaunchType.FROM_CHROME_UI, TabLaunchType.FROM_RESTORE -> hasParent;
            case TabLaunchType.FROM_SYNC_BACKGROUND ->
                    hasParent && ChromeFeatureList.sSendTabToSelfSwitchToParentOnBack.isEnabled();
            default -> false;
        };
    }

    /**
     * Returns true if a tab launched with the given type bypasses order controller insertion index
     * calculation and defers directly to {@code TabList.INVALID_TAB_INDEX}.
     *
     * @param type The launch type to inspect.
     * @return True if insertion order calculation is bypassed.
     */
    public static boolean bypassesInsertionOrderCalculation(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_BROWSER_ACTIONS, TabLaunchType.FROM_RECENT_TABS -> true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type represents an explicit user action to open a New Tab Page.
     *
     * @param type The launch type to inspect.
     * @return True if the launch type is an explicit user NTP launch.
     */
    public static boolean isExplicitUserNtpLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_CHROME_UI,
                    TabLaunchType.FROM_TAB_GROUP_UI,
                    TabLaunchType.FROM_TAB_SWITCHER_UI,
                    TabLaunchType.FROM_TIPS_NOTIFICATIONS ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type represents a tab added remotely in the background via sync or
     * tab group collaboration.
     *
     * @param type The launch type to inspect.
     * @return True if the tab is a remote background launch.
     */
    public static boolean isRemoteBackgroundLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_SYNC_BACKGROUND,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if adding a tab with the given launch type should cause the TabListEditor
     * selection UI to navigate back / close.
     *
     * @param type The launch type to inspect.
     * @return True if the TabListEditor should navigate back.
     */
    public static boolean shouldNavigateBackFromTabListEditor(@TabLaunchType int type) {
        assertValidLaunchType(type);
        // When tab is added due to
        // 1) multi-window close
        // 2) moving between multiple windows
        // 3) NTP at startup
        // force hiding the selection editor.
        return switch (type) {
            case TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_REPARENTING,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_STARTUP ->
                    true;
            default -> false;
        };
    }
}
