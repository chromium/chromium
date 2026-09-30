// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Map;
import java.util.Set;

/** Unit tests for {@link TabLaunchTypeUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabLaunchTypeUtilsUnitTest {
    private static final Set<Integer> BACKGROUND_TYPES =
            Set.of(
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_RECENT_TABS,
                    TabLaunchType.FROM_SYNC_BACKGROUND,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND,
                    TabLaunchType.FROM_OMNIBOX_BACKGROUND);

    private static final Set<Integer> RESTORE_TYPES =
            Set.of(
                    TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabLaunchType.FROM_RESTORE_TABS_UI);

    private static final Set<Integer> GROUPED_TYPES =
            Set.of(
                    TabLaunchType.FROM_TAB_GROUP_UI,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP);

    private static final Set<Integer> ADJACENT_TYPES =
            Set.of(
                    TabLaunchType.FROM_LINK,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND);

    private static final Set<Integer> REPARENTING_TYPES =
            Set.of(TabLaunchType.FROM_REPARENTING, TabLaunchType.FROM_REPARENTING_BACKGROUND);

    private static final Set<Integer> SKIP_TAB_CREATING_ANIMATION_TYPES =
            Set.of(
                    TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_REPARENTING,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_EXTERNAL_APP,
                    TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                    TabLaunchType.FROM_STARTUP,
                    TabLaunchType.FROM_APP_WIDGET,
                    TabLaunchType.FROM_SYNC_BACKGROUND);

    private static final Map<Integer, String> EXPECTED_HISTOGRAM_KEYS =
            Map.ofEntries(
                    Map.entry(TabLaunchType.FROM_LINK, "Link"),
                    Map.entry(TabLaunchType.FROM_EXTERNAL_APP, "ExternalApp"),
                    Map.entry(TabLaunchType.FROM_CHROME_UI, "ChromeUI"),
                    Map.entry(TabLaunchType.FROM_RESTORE, "Restore"),
                    Map.entry(TabLaunchType.FROM_LONGPRESS_FOREGROUND, "LongressForeground"),
                    Map.entry(TabLaunchType.FROM_LONGPRESS_BACKGROUND, "LongpressBackground"),
                    Map.entry(TabLaunchType.FROM_REPARENTING, "Reparenting"),
                    Map.entry(TabLaunchType.FROM_LAUNCHER_SHORTCUT, "LauncherShortcut"),
                    Map.entry(
                            TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION,
                            "SpeculativeBackgroundCreation"),
                    Map.entry(TabLaunchType.FROM_BROWSER_ACTIONS, "BrowserActions"),
                    Map.entry(TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB, "NewIncognitoTab"),
                    Map.entry(TabLaunchType.FROM_STARTUP, "Startup"),
                    Map.entry(
                            TabLaunchType.FROM_SESSION_STARTUP_WITH_URLS_PREF,
                            "SessionStartupWithUrlsPref"),
                    Map.entry(TabLaunchType.FROM_START_SURFACE, "StartSurface"),
                    Map.entry(TabLaunchType.FROM_TAB_GROUP_UI, "TabGroupUI"),
                    Map.entry(
                            TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                            "LongpressBackgroundInGroup"),
                    Map.entry(TabLaunchType.FROM_APP_WIDGET, "AppWidget"),
                    Map.entry(TabLaunchType.FROM_LONGPRESS_INCOGNITO, "LongpressIncognito"),
                    Map.entry(TabLaunchType.FROM_RECENT_TABS, "RecentTabs"),
                    Map.entry(TabLaunchType.FROM_READING_LIST, "ReadingList"),
                    Map.entry(TabLaunchType.FROM_TAB_SWITCHER_UI, "TabSwitcherUI"),
                    Map.entry(TabLaunchType.FROM_RESTORE_TABS_UI, "RestoreTabsUI"),
                    Map.entry(TabLaunchType.FROM_OMNIBOX, "Omnibox"),
                    Map.entry(TabLaunchType.FROM_OMNIBOX_BACKGROUND, "OmniboxBackground"),
                    Map.entry(TabLaunchType.UNSET, "Unset"),
                    Map.entry(TabLaunchType.FROM_SYNC_BACKGROUND, "SyncBackground"),
                    Map.entry(TabLaunchType.FROM_RECENT_TABS_FOREGROUND, "RecentTabsForeground"),
                    Map.entry(
                            TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                            "CollaborationBackgroundInGroup"),
                    Map.entry(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND, "BookmarkBarBackground"),
                    Map.entry(TabLaunchType.FROM_REPARENTING_BACKGROUND, "ReparentingBackground"),
                    Map.entry(
                            TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                            "HistoryNavigationBackground"),
                    Map.entry(
                            TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND,
                            "HistoryNavigationBackground"),
                    Map.entry(
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                            "LongpressForegroundInGroup"),
                    Map.entry(TabLaunchType.FROM_TAB_LIST_INTERFACE, "TabListInterface"),
                    Map.entry(TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW, "LinkToNewWindow"),
                    Map.entry(TabLaunchType.FROM_TIPS_NOTIFICATIONS, "TipsNotifications"),
                    Map.entry(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND,
                            "TabListInterfaceBackground"));

    @Test
    public void testEnumSizeConstant() {
        assertEquals("TabLaunchType.SIZE is expected to be 37", 37, TabLaunchType.SIZE);
        assertEquals("Expected 37 histogram key mappings", 37, EXPECTED_HISTOGRAM_KEYS.size());
    }

    @Test
    public void testPredicatesAcrossAllLaunchTypes() {
        for (@TabLaunchType int type = 0; type < TabLaunchType.SIZE; type++) {
            assertEquals(
                    "isBackgroundLaunch mismatch for type " + type,
                    BACKGROUND_TYPES.contains(type),
                    TabLaunchTypeUtils.isBackgroundLaunch(type));
            assertEquals(
                    "isRestoreLaunch mismatch for type " + type,
                    RESTORE_TYPES.contains(type),
                    TabLaunchTypeUtils.isRestoreLaunch(type));
            assertEquals(
                    "shouldLaunchAsGroupedTab mismatch for type " + type,
                    GROUPED_TYPES.contains(type),
                    TabLaunchTypeUtils.shouldLaunchAsGroupedTab(type));
            assertEquals(
                    "shouldOpenAdjacent mismatch for type " + type,
                    ADJACENT_TYPES.contains(type),
                    TabLaunchTypeUtils.shouldOpenAdjacent(type));
            assertEquals(
                    "isReparentingLaunch mismatch for type " + type,
                    REPARENTING_TYPES.contains(type),
                    TabLaunchTypeUtils.isReparentingLaunch(type));
            boolean expectedBgOrRestore =
                    BACKGROUND_TYPES.contains(type) || RESTORE_TYPES.contains(type);
            assertEquals(
                    "willAddedTabBeSelected(false, false) mismatch for type " + type,
                    type != TabLaunchType.FROM_RESTORE && !expectedBgOrRestore,
                    TabLaunchTypeUtils.willAddedTabBeSelected(
                            type, /* isTabIncognito= */ false, /* isIncognitoSelected= */ false));
            assertEquals(
                    "willAddedTabBeSelected(false, true) mismatch for type " + type,
                    type != TabLaunchType.FROM_RESTORE && !expectedBgOrRestore,
                    TabLaunchTypeUtils.willAddedTabBeSelected(
                            type, /* isTabIncognito= */ false, /* isIncognitoSelected= */ true));
            assertEquals(
                    "willAddedTabBeSelected(true, false) mismatch for type " + type,
                    type != TabLaunchType.FROM_RESTORE,
                    TabLaunchTypeUtils.willAddedTabBeSelected(
                            type, /* isTabIncognito= */ true, /* isIncognitoSelected= */ false));
            assertEquals(
                    "willAddedTabBeSelected(true, true) mismatch for type " + type,
                    type != TabLaunchType.FROM_RESTORE && !expectedBgOrRestore,
                    TabLaunchTypeUtils.willAddedTabBeSelected(
                            type, /* isTabIncognito= */ true, /* isIncognitoSelected= */ true));
            assertEquals(
                    "shouldSkipTabCreatingAnimation mismatch for type " + type,
                    SKIP_TAB_CREATING_ANIMATION_TYPES.contains(type),
                    TabLaunchTypeUtils.shouldSkipTabCreatingAnimation(type));
            assertEquals(
                    "tabLaunchTypeToHistogramKey mismatch for type " + type,
                    EXPECTED_HISTOGRAM_KEYS.get(type),
                    TabLaunchTypeUtils.tabLaunchTypeToHistogramKey(type));
            assertNotEquals(
                    "tabLaunchTypeToHistogramKey returned TypeUnknown for type " + type,
                    "TypeUnknown",
                    TabLaunchTypeUtils.tabLaunchTypeToHistogramKey(type));
        }
    }

    @Test
    public void testPredicatesWithOutOfBoundsAndInvalidTypes() {
        int[] invalidTypes =
                new int[] {-1, TabLaunchType.SIZE, 100, Integer.MIN_VALUE, Integer.MAX_VALUE};
        for (int type : invalidTypes) {
            assertThrows(
                    "isBackgroundLaunch should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.isBackgroundLaunch(type));
            assertThrows(
                    "isRestoreLaunch should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.isRestoreLaunch(type));
            assertThrows(
                    "shouldLaunchAsGroupedTab should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.shouldLaunchAsGroupedTab(type));
            assertThrows(
                    "shouldOpenAdjacent should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.shouldOpenAdjacent(type));
            assertThrows(
                    "isReparentingLaunch should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.isReparentingLaunch(type));
            assertThrows(
                    "willAddedTabBeSelected should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.willAddedTabBeSelected(type, false, false));
            assertThrows(
                    "shouldSkipTabCreatingAnimation should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.shouldSkipTabCreatingAnimation(type));
            assertThrows(
                    "tabLaunchTypeToHistogramKey should assert for invalid type " + type,
                    AssertionError.class,
                    () -> TabLaunchTypeUtils.tabLaunchTypeToHistogramKey(type));
        }
    }
}
