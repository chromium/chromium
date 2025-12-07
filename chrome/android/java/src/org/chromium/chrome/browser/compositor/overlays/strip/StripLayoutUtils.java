// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.HapticFeedbackConstants;
import android.view.View;

import org.chromium.base.DeviceInfo;
import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

@NullMarked
public class StripLayoutUtils {
    // Position Constants.
    // The bottom indicator should align with the contents of the last tab in group. This value is
    // calculated as:
    // closeButtonEndPadding(10) + tabContainerEndPadding(16) + groupTitleStartMargin(13)
    //         - overlap(28-16) =
    public static final float TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET = 27f;

    // Tab width constants.
    private static final float MIN_TAB_WIDTH_DESKTOP_DP = 76f;
    private static final float MIN_TAB_WIDTH_TABLET_DP = 108f;
    public static final float MIN_TAB_WIDTH_DP =
            shouldApplyMoreDensity() ? MIN_TAB_WIDTH_DESKTOP_DP : MIN_TAB_WIDTH_TABLET_DP;
    public static final float MAX_TAB_WIDTH_DP = TabUiThemeUtil.getMaxTabStripTabWidthDp();
    public static final float TAB_OVERLAP_WIDTH_DP = 28f;

    // Pinned tab width.
    public static final float PINNED_TAB_WIDTH_DP = MIN_TAB_WIDTH_DESKTOP_DP;

    // Button size constants.
    public static final float BUTTON_BACKGROUND_SIZE_DP = 32f;
    public static final float BUTTON_TOUCH_TARGET_SIZE_DP =
            shouldApplyMoreDensity() ? BUTTON_BACKGROUND_SIZE_DP : 48f;

    // Animation Constants.
    public static final int ANIM_TAB_MOVE_MS = 125;
    public static final int ANIM_TAB_SLIDE_OUT_MS = 250;

    // Reorder Constants.
    public static final long INVALID_TIME = 0L;
    public static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;

    // ============================================================================================
    // Tab group helpers
    // ============================================================================================

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given tabs.
     * @param tab1 A {@link Tab} that we're comparing.
     * @param tab2 A {@link Tab} that we're comparing.
     * @return Whether the two tabs are not related, and at least one is grouped.
     */
    public static boolean notRelatedAndEitherTabInGroup(
            TabGroupModelFilter modelFilter, Tab tab1, Tab tab2) {
        return !Objects.equals(tab1.getTabGroupId(), tab2.getTabGroupId())
                && (modelFilter.isTabInTabGroup(tab1) || modelFilter.isTabInTabGroup(tab2));
    }

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given group.
     * @param tabId The ID of the given tab.
     * @return {@code true} if the tab is grouped and is the last tab in the group. False otherwise.
     */
    public static boolean isLastTabInGroup(TabGroupModelFilter modelFilter, int tabId) {
        Tab tab = modelFilter.getTabModel().getTabById(tabId);
        if (tab == null) {
            return false;
        }
        return modelFilter.isTabInTabGroup(tab)
                && modelFilter.getTabCountForGroup(tab.getTabGroupId()) == 1;
    }

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given group.
     * @param stripLayoutGroupTitle The {@link StripLayoutGroupTitle}
     * @return The number of tabs in the group associated with the group title.
     */
    public static int getNumOfTabsInGroup(
            @Nullable TabGroupModelFilter modelFilter,
            StripLayoutGroupTitle stripLayoutGroupTitle) {
        return modelFilter == null
                ? 0
                : modelFilter.getTabCountForGroup(stripLayoutGroupTitle.getTabGroupId());
    }

    /**
     * @param modelFilter The {@link TabGroupModelFilter} that holds the given group.
     * @param tabModel The {@link TabModel} that holds the give tab.
     * @param stripTab The {@link StripLayoutTab}
     * @return Whether the given tab is at a non-last position in any group.
     */
    public static boolean isNonTrailingTabInGroup(
            TabGroupModelFilter modelFilter, TabModel tabModel, StripLayoutTab stripTab) {
        Tab tab = assumeNonNull(tabModel.getTabById(stripTab.getTabId()));
        if (modelFilter.isTabInTabGroup(tab)) {
            List<Tab> relatedTabs = modelFilter.getRelatedTabList(tab.getId());
            Tab lastTab = relatedTabs.get(relatedTabs.size() - 1);
            return tab.getId() != lastTab.getId();
        }
        return false;
    }

    /**
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param view The {@link StripLayoutView} to check whether is the last pinned tab.
     * @return Whether the view is the last pinned tab.
     */
    public static boolean isLastPinnedTab(StripLayoutTab[] stripTabs, StripLayoutView view) {
        StripLayoutTab tab = (view instanceof StripLayoutTab) ? (StripLayoutTab) view : null;
        if (tab == null) return false;
        for (int i = 0; i < stripTabs.length; i++) {
            if (!stripTabs[i].getIsPinned()) {
                if (i > 0) {
                    return tab == stripTabs[i - 1];
                } else {
                    break;
                }
            }
        }
        return false;
    }

    /**
     * @param groupTitles A list of {@link StripLayoutGroupTitle}.
     * @param tabGroupId The tab group ID for the tab group title we're searching for.
     * @return The {@link StripLayoutGroupTitle} with the given root ID. {@code null} otherwise.
     */
    public static @Nullable StripLayoutGroupTitle findGroupTitle(
            StripLayoutGroupTitle[] groupTitles, @Nullable Token tabGroupId) {
        if (tabGroupId == null) return null;
        for (int i = 0; i < groupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = groupTitles[i];
            if (tabGroupId.equals(groupTitle.getTabGroupId())) return groupTitle;
        }
        return null;
    }

    /**
     * @param groupTitles A list of {@link StripLayoutGroupTitle}.
     * @param collaborationId The sharing ID associated with the group.
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @return The {@link StripLayoutGroupTitle} with the given tab group ID. {@code null}
     *     otherwise.
     */
    static @Nullable StripLayoutGroupTitle findGroupTitleByCollaborationId(
            StripLayoutGroupTitle[] groupTitles,
            String collaborationId,
            TabGroupSyncService tabGroupSyncService) {
        for (StripLayoutGroupTitle groupTitle : groupTitles) {
            SavedTabGroup savedTabGroup =
                    tabGroupSyncService.getGroup(new LocalTabGroupId(groupTitle.getTabGroupId()));
            if (savedTabGroup != null
                    && savedTabGroup.collaborationId != null
                    && savedTabGroup.collaborationId.equals(collaborationId)) {
                return groupTitle;
            }
        }
        return null;
    }

    /**
     * @param groupTitle The tab group title indicator {@link StripLayoutGroupTitle}.
     * @param numTabsInGroup Number of tabs in the tab group.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @return The total width of the group title and the number of tabs associated with it.
     */
    public static float calculateBottomIndicatorWidth(
            StripLayoutGroupTitle groupTitle, int numTabsInGroup, float effectiveTabWidth) {
        if (groupTitle == null || groupTitle.isCollapsed() || numTabsInGroup == 0) {
            return 0.f;
        }
        float totalTabWidth =
                effectiveTabWidth * numTabsInGroup - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
        return groupTitle.getWidth() + totalTabWidth;
    }

    /** Returns a list of the {@link StripLayoutTab}s in a given group. */
    public static List<StripLayoutTab> getGroupedTabs(
            TabModel tabModel, StripLayoutTab[] stripTabs, Token tabGroupId) {
        ArrayList<StripLayoutTab> groupedTabs = new ArrayList<>();
        for (int i = 0; i < stripTabs.length; ++i) {
            final StripLayoutTab stripTab = stripTabs[i];
            final Tab tab = tabModel.getTabById(stripTab.getTabId());
            if (tab != null && tabGroupId.equals(tab.getTabGroupId())) groupedTabs.add(stripTab);
        }
        return groupedTabs;
    }

    /** Returns the number of non-closing {@link StripLayoutTab}s in a given group. */
    public static int getNumLiveGroupedTabs(
            TabModel tabModel, StripLayoutTab[] stripTabs, Token tabGroupId) {
        // TODO(crbug.com/443337907): This will be obsolete once we immediately close in the
        //  TabModel, as we could then instead use TabGroupModelFilter#getTabCountForGroup.
        List<StripLayoutTab> groupedTabs = getGroupedTabs(tabModel, stripTabs, tabGroupId);

        int numLiveGroupedTabs = 0;
        for (StripLayoutTab tab : groupedTabs) {
            if (!tab.isDying()) numLiveGroupedTabs++;
        }
        return numLiveGroupedTabs;
    }

    // ============================================================================================
    // Tab util methods
    // ============================================================================================

    /**
     * @param tabWidthSupplier supplies the cached tab width for non-pinned tabs
     * @param isPinned Whether the tab is pinned; currently always false for grouped tabs.
     * @return Returns half of {@code mEffectiveTabWidth}.
     */
    public static float getHalfTabWidth(Supplier<Float> tabWidthSupplier, boolean isPinned) {
        return getEffectiveTabWidth(tabWidthSupplier, isPinned) / 2;
    }

    /**
     * @param tabWidthSupplier supplies the cached tab width for non-pinned tabs.
     * @param isPinned Whether the tab is pinned; currently always false for grouped tabs.
     * @return Returns the current effective tab width (accounting for overlap).
     */
    public static float getEffectiveTabWidth(Supplier<Float> tabWidthSupplier, boolean isPinned) {
        float tabWidth = isPinned ? PINNED_TAB_WIDTH_DP : tabWidthSupplier.get();
        return (tabWidth - TAB_OVERLAP_WIDTH_DP);
    }

    /**
     * @param x raw drag X in strip coordinates.
     * @param tabWidthSupplier supplies the cached tab width for non-pinned tabs.
     * @param isPinned Whether the tab is pinned; currently always false for grouped tabs.
     * @return Returns x shifted by half tab width to accommodate for tab drop.
     */
    public static float adjustXForTabDrop(
            float x, Supplier<Float> tabWidthSupplier, boolean isPinned) {
        return x
                - MathUtils.flipSignIf(
                        StripLayoutUtils.getHalfTabWidth(tabWidthSupplier, isPinned),
                        LocalizationUtils.isLayoutRtl());
    }

    // ============================================================================================
    // StripLayoutView/Tab array util methods
    // ============================================================================================

    /**
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param id The ID of the {@link StripLayoutTab} we're searching for.
     * @return The {@link StripLayoutTab}'s index. {@link TabModel#INVALID_TAB_INDEX} if not found.
     */
    public static int findIndexForTab(StripLayoutTab[] stripTabs, int id) {
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
    public static @Nullable StripLayoutTab findTabById(StripLayoutTab[] stripTabs, int id) {
        if (stripTabs == null) return null;
        for (int i = 0; i < stripTabs.length; i++) {
            if (stripTabs[i].getTabId() == id) return stripTabs[i];
        }
        return null;
    }

    /**
     * Returns a visible {@link StripLayoutView} at the given x-position, or {@code null} if none.
     *
     * @param views The list of {@link StripLayoutView}.
     * @param x The x position to use to retrieve view.
     * @param includeGroupTitles Whether to include group title when finding view.
     * @return View at x position. {@code null} if no view at position or if input criteria not met.
     */
    public static @Nullable StripLayoutView findViewAtPositionX(
            StripLayoutView[] views, float x, boolean includeGroupTitles) {
        for (StripLayoutView view : views) {
            float leftEdge;
            float rightEdge;
            if (view instanceof StripLayoutTab tab) {
                leftEdge = tab.getTouchTargetLeft();
                rightEdge = tab.getTouchTargetRight();
            } else {
                if (!includeGroupTitles) continue;
                leftEdge = view.getDrawX();
                rightEdge = leftEdge + view.getWidth();
            }
            if (LocalizationUtils.isLayoutRtl()) {
                leftEdge -= view.getTrailingMargin();
            } else {
                rightEdge += view.getTrailingMargin();
            }

            if (view.isVisible() && leftEdge <= x && x <= rightEdge) {
                return view;
            }
        }

        return null;
    }

    /**
     * @param views The list of {@link StripLayoutView}.
     * @return An array of the views that have not been dragged off the strip.
     */
    public static StripLayoutView[] getViewsOnStrip(StripLayoutView[] views) {
        int numViewsOnStrip = views.length;
        for (int i = 0; i < views.length; ++i) {
            if (views[i].isDraggedOffStrip()) --numViewsOnStrip;
        }

        int index = 0;
        StripLayoutView[] viewsOnStrip = new StripLayoutView[numViewsOnStrip];
        for (int i = 0; i < views.length; ++i) {
            final StripLayoutView view = views[i];
            if (!view.isDraggedOffStrip()) viewsOnStrip[index++] = view;
        }

        return viewsOnStrip;
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

    public static <T> boolean arrayContains(T[] array, T desiredElem) {
        for (int i = 0; i < array.length; i++) {
            final T elem = array[i];
            if (elem == desiredElem) return true;
        }
        return false;
    }

    /**
     * Records the number of tabs that are currently multi-selected.
     *
     * @param tabModel The {@link TabModel}.
     */
    public static void recordTabMultiSelectionTabCount(@Nullable TabModel tabModel) {
        if (!ChromeFeatureList.sAndroidTabHighlighting.isEnabled() || tabModel == null) return;
        RecordHistogram.recordCount100Histogram(
                "Tabs.Selections.Count",tabModel.getMultiSelectedTabsCount());
    }

    // Other methods.

    public static void performHapticFeedback(View view) {
        if (view == null) return;
        view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
    }

    public static boolean skipTabEdgePositionCalculation(StripLayoutTab tab) {
        return (tab.isDying() && !ChromeFeatureList.sTabletTabStripAnimation.isEnabled())
                || tab.isDraggedOffStrip();
    }

    public static boolean shouldApplyMoreDensity() {
        return ChromeFeatureList.sTabStripDensityChangeAndroid.isEnabled()
                && DeviceInfo.isDesktop();
    }

    // Testing booleans
    public static boolean isTabHighlightingForceCtrlClick() {
        return ChromeFeatureList.sAndroidTabHighlightingForceCtrlClick.getValue();
    }

    public static boolean isTabHighlightingForceShiftClick() {
        return ChromeFeatureList.sAndroidTabHighlightingForceShiftClick.getValue();
    }

    public static boolean isTabHighlightingTestingEnabled() {
        return isTabHighlightingForceCtrlClick() || isTabHighlightingForceShiftClick();
    }

    public static boolean isTabPinningFromStripEnabled() {
        return ChromeFeatureList.sAndroidPinnedTabsTabletTabStrip.isEnabled();
    }
}
