// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/** Utility methods for querying behavioral traits and predicates of {@link TabLaunchType}. */
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
}
