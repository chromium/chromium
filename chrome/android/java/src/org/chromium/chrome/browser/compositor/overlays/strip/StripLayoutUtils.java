// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;

public class StripLayoutUtils {
    // Position Constants.
    // The bottom indicator should align with the contents of the last tab in group. This value is
    // calculated as:
    // closeButtonEndPadding(10) + tabContainerEndPadding(16) + groupTitleStartMargin(13)
    //         - overlap(28-16) =
    @VisibleForTesting static final float TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET = 27.f;
    static final float MIN_TAB_WIDTH_DP = 108.f;
    static final float MAX_TAB_WIDTH_DP = TabUiThemeUtil.getMaxTabStripTabWidthDp();
    static final float TAB_OVERLAP_WIDTH_DP = 28.f;

    // Animation Constants.
    static final int ANIM_TAB_MOVE_MS = 125;
    static final int ANIM_TAB_SLIDE_OUT_MS = 250;

    // Reorder Constants.
    static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;
    static final long INVALID_TIME = 0L;

    // ============================================================================================
    // Tab group helpers
    // ============================================================================================

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given tabs.
     * @param tab1 A {@link Tab} that we're comparing.
     * @param tab2 A {@link Tab} that we're comparing.
     * @return Whether the two tabs are not related, and at least one is grouped.
     */
    static boolean notRelatedAndEitherTabInGroup(
            TabGroupModelFilter modelFilter, @NonNull Tab tab1, @NonNull Tab tab2) {
        return tab1.getRootId() != tab2.getRootId()
                && (modelFilter.isTabInTabGroup(tab1) || modelFilter.isTabInTabGroup(tab2));
    }

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given group.
     * @param tabId The ID of the given tab.
     * @return {@code true} if the tab is grouped and is the last tab in the group. False otherwise.
     */
    static boolean isLastTabInGroup(TabGroupModelFilter modelFilter, int tabId) {
        Tab tab = modelFilter.getTabModel().getTabById(tabId);
        if (tab == null) {
            return false;
        }
        return modelFilter.isTabInTabGroup(tab)
                && modelFilter.getRelatedTabCountForRootId(tab.getRootId()) == 1;
    }

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given group.
     * @param stripLayoutGroupTitle The {@link StripLayoutGroupTitle}
     * @return The number of tabs in the group associated with the group title.
     */
    static int getNumOfTabsInGroup(
            TabGroupModelFilter modelFilter, StripLayoutGroupTitle stripLayoutGroupTitle) {
        if (stripLayoutGroupTitle == null) {
            return 0;
        }
        return modelFilter.getRelatedTabCountForRootId(stripLayoutGroupTitle.getRootId());
    }

    /**
     * @param model The {@link TabModel} that holds the given tab.
     * @param stripTab The {@link StripLayoutTab} to find the root ID for.
     * @return The root ID for the given tab. {@code Tab.INVALID_TAB_ID} if no {@link Tab} found.
     */
    static int getRootId(TabModel model, StripLayoutTab stripTab) {
        if (stripTab == null) return Tab.INVALID_TAB_ID;
        Tab tab = model.getTabById(stripTab.getTabId());
        return tab == null ? Tab.INVALID_TAB_ID : tab.getRootId();
    }

    /**
     * @param groupTitles A list of {@link StripLayoutGroupTitle}.
     * @param rootId The root ID for the tab group title we're searching for.
     * @return The {@link StripLayoutGroupTitle} with the given root ID. {@code null} otherwise.
     */
    static StripLayoutGroupTitle findGroupTitle(StripLayoutGroupTitle[] groupTitles, int rootId) {
        for (int i = 0; i < groupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = groupTitles[i];
            if (groupTitle.getRootId() == rootId) return groupTitle;
        }
        return null;
    }

    /**
     * @param groupTitle The tab group title indicator {@link StripLayoutGroupTitle}.
     * @param numTabsInGroup Number of tabs in the tab group.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @return The total width of the group title and the number of tabs associated with it.
     */
    static float calculateBottomIndicatorWidth(
            StripLayoutGroupTitle groupTitle, int numTabsInGroup, float effectiveTabWidth) {
        if (groupTitle == null || groupTitle.isCollapsed() || numTabsInGroup == 0) {
            return 0.f;
        }
        float totalTabWidth =
                effectiveTabWidth * numTabsInGroup - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
        return groupTitle.getWidth() + totalTabWidth;
    }

    // ============================================================================================
    // StripLayoutView/Tab array util methods
    // ============================================================================================

    /**
     * @param stripViews The list of all of the tab strip's views.
     * @param stripTabs The list of all of the tab strip's tabs.
     * @param stripTabIndex The index in the list of tabs.
     * @return The index in the list of views.
     */
    static int findStripViewIndexForStripTab(
            StripLayoutView[] stripViews, StripLayoutTab[] stripTabs, int stripTabIndex) {
        if (stripTabIndex == TabModel.INVALID_TAB_INDEX) {
            return TabModel.INVALID_TAB_INDEX;
        }
        assert stripTabIndex < stripTabs.length;
        StripLayoutTab curTab = stripTabs[stripTabIndex];
        if (stripViews == null || curTab == null) return TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < stripViews.length; i++) {
            if (stripViews[i] instanceof StripLayoutTab tab && curTab == tab) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param id The ID of the {@link StripLayoutTab} we're searching for.
     * @return The {@link StripLayoutTab}'s index. {@link TabModel#INVALID_TAB_INDEX} if not found.
     */
    static int findIndexForTab(StripLayoutTab[] stripTabs, int id) {
        if (stripTabs == null || id == Tab.INVALID_TAB_ID) return TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < stripTabs.length; i++) {
            final StripLayoutTab stripTab = stripTabs[i];
            if (stripTab.getTabId() == id) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param id The ID of the {@link StripLayoutTab} we're searching for.
     * @return The {@link StripLayoutTab}. {@code null} if not found.
     */
    static @Nullable StripLayoutTab findTabById(StripLayoutTab[] stripTabs, int id) {
        if (stripTabs == null) return null;
        for (int i = 0; i < stripTabs.length; i++) {
            if (stripTabs[i].getTabId() == id) return stripTabs[i];
        }
        return null;
    }

    // ============================================================================================
    // Array helpers
    // ============================================================================================

    /**
     * Moves an element in the given array.
     *
     * @param array The given array.
     * @param oldIndex The original index of the item we're moving.
     * @param newIndex The desired index of the item we're moving.
     */
    static <T> void moveElement(T[] array, int oldIndex, int newIndex) {
        if (oldIndex <= newIndex) {
            moveElementUp(array, oldIndex, newIndex);
        } else {
            moveElementDown(array, oldIndex, newIndex);
        }
    }

    private static <T> void moveElementUp(T[] array, int oldIndex, int newIndex) {
        assert oldIndex <= newIndex;
        if (oldIndex == newIndex || oldIndex + 1 == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex; i < newIndex - 1; i++) {
            array[i] = array[i + 1];
        }
        array[newIndex - 1] = elem;
    }

    private static <T> void moveElementDown(T[] array, int oldIndex, int newIndex) {
        assert oldIndex >= newIndex;
        if (oldIndex == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex - 1; i >= newIndex; i--) {
            array[i + 1] = array[i];
        }
        array[newIndex] = elem;
    }

    // Other methods.

    static void performHapticFeedback(View view) {
        if (view == null) return;
        view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
    }
}
