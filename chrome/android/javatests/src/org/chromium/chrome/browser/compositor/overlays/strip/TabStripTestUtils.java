// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.util.TabStripUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

// Test helper for Tab Strip features.
public class TabStripTestUtils {

    /**
     * Creates tabs to reach the target count in the selected model.Regular mode already has one
     * default tab, so this creates (numOfTabs - 1); incognito creates numOfTabs. Note: repeated
     * calls in regular mode accumulate (e.g., N then M results in N+M-1).
     *
     * @param activity The hosting ChromeTabbedActivity.
     * @param isIncognito Whether the tab is in incognito.
     * @param numOfTabs Total number of tabs desired after creation.
     */
    public static void createTabs(
            ChromeTabbedActivity activity, boolean isIncognito, int numOfTabs) {
        TabUiTestHelper.createTabs(activity, isIncognito, numOfTabs);
    }

    /**
     * Creates a tab group from two tabs (by index) in the selected model. Note: with multiple
     * groups, the title may not appear at firstIndex, the assert needs to be updated accordingly.
     *
     * @param activity The hosting ChromeTabbedActivity.
     * @param isIncognito Whether the tab is in incognito.
     * @param firstIndex The first tab index to group.
     * @param secondIndex The second tab index to group.
     */
    public static void createTabGroup(
            ChromeTabbedActivity activity, boolean isIncognito, int firstIndex, int secondIndex) {
        // 1. Assert the correct tab model is selected.
        assertEquals(
                "The wrong tab model is selected",
                isIncognito,
                activity.getTabModelSelector().isIncognitoSelected());

        // 2. Group the two tabs.
        List<Tab> tabGroup =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new ArrayList<>(
                                        Arrays.asList(
                                                activity.getCurrentTabModel().getTabAt(firstIndex),
                                                activity.getCurrentTabModel()
                                                        .getTabAt(secondIndex))));
        TabUiTestHelper.createTabGroup(activity, isIncognito, tabGroup);
        StripLayoutHelper stripLayoutHelper = getActiveStripLayoutHelper(activity);
        StripLayoutView[] views = stripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(
                "The view should be a group title.",
                views[firstIndex] instanceof StripLayoutGroupTitle);
    }

    /**
     * @param activity The hosting ChromeTabbedActivity.
     * @param isIncognito Whether the tab is in incognito.
     * @return The {@link TabGroupModelFilter} to act on.
     */
    public static TabGroupModelFilter getTabGroupModelFilter(
            ChromeTabbedActivity activity, boolean isIncognito) {
        return activity.getTabModelSelector()
                .getTabGroupModelFilterProvider()
                .getTabGroupModelFilter(isIncognito);
    }

    /**
     * @param activity The main activity that contains the TabStrips.
     * @return The TabStrip for the specified model.
     */
    public static StripLayoutHelper getActiveStripLayoutHelper(ChromeTabbedActivity activity) {
        StripLayoutHelperManager manager = TabStripUtils.getStripLayoutHelperManager(activity);
        if (manager != null) {
            return manager.getActiveStripLayoutHelper();
        }
        return null;
    }
}
