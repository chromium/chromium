// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

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
                    TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND);

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

    @Test
    public void testEnumSizeConstant() {
        assertEquals("TabLaunchType.SIZE is expected to be 36", 36, TabLaunchType.SIZE);
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
        }
    }
}
