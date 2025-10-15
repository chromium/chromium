// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.floatThat;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link ExternalViewDragDropReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class ExternalViewDragDropReorderStrategyTest extends ReorderStrategyTestBase {
    // Constants
    private static final int INTERACTING_VIEW_ID = TAB_ID4;

    // Data
    private StripLayoutTab mStripTab1;
    private StripLayoutTab mStripTab2;
    private StripLayoutTab mStripTab3;

    // Target
    private ExternalViewDragDropReorderStrategy mStrategy;

    @Before
    @Override
    public void setup() {
        super.setup();
        mStrategy =
                new ExternalViewDragDropReorderStrategy(
                        mReorderDelegate,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier,
                        mLastReorderScrollTimeSupplier);
        setupDragDropState(/* isGroupDrag= */ false);
    }

    @Override
    protected void setupStripViews() {
        // Data = [Tab1]  [Tab2]  [InteractingGroupTitle][InteractingTab]  [Tab3]
        mStripTab1 = buildStripTab(TAB_ID1, 0);
        mInteractingGroupTitle = buildGroupTitle(GROUP_ID1, TAB_WIDTH);
        mInteractingTab = buildStripTab(INTERACTING_VIEW_ID, 2 * TAB_WIDTH);
        mStripTab2 = buildStripTab(TAB_ID2, 3 * TAB_WIDTH);
        mStripTab3 = buildStripTab(TAB_ID3, 4 * TAB_WIDTH);

        mStripTabs = new StripLayoutTab[] {mStripTab1, mInteractingTab, mStripTab2, mStripTab3};
        mGroupTitles = new StripLayoutGroupTitle[] {mInteractingGroupTitle};
        mStripViews =
                new StripLayoutView[] {
                    mStripTab1, mInteractingGroupTitle, mInteractingTab, mStripTab2, mStripTab3
                };
    }

    @Test
    public void testStartReorder() {
        // Call
        startReorder(mInteractingTab);

        // Verify - Animations
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());

        // Verify - No edge margins
        verifyNoInteractions(mScrollDelegate);
        assertEquals("Should not have end margin.", 0, mStripTab3.getTrailingMargin(), EPSILON);

        // Verify - Correct hovered view
        verifyViewHovered(mInteractingTab);
        verifyViewNotHovered(mStripTab1);
        verifyViewNotHovered(mStripTab2);
        verifyViewNotHovered(mStripTab3);
    }

    @Test
    public void testStartReorder_overGroup() {
        // Mock interacting view in a group
        mockTabInGroup(INTERACTING_VIEW_ID);

        // Call
        startReorder(mInteractingTab);

        // Verify - Animations
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());

        // Verify - Bottom indicator width
        float initialIndicatorWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        mInteractingGroupTitle, /* numTabsInGroup= */ 1, EFFECTIVE_TAB_WIDTH);
        float expectedIndicatorWidth = initialIndicatorWidth + mInteractingTab.getTrailingMargin();
        assertEquals(
                "Bottom indicator should now account for trailing margin.",
                expectedIndicatorWidth,
                mInteractingGroupTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testStartReorder_setStartMargin() {
        // Mock first tab grouped then start reorder
        mockTabInGroup(mStripTab1.getTabId());
        startReorder(mInteractingTab);

        // Verify
        verify(mScrollDelegate).setReorderStartMargin(floatThat(f -> f > 0f));
    }

    @Test
    public void testStartReorder_setEndMargin() {
        // Mock last tab grouped then start reorder
        mockTabInGroup(mStripTab3.getTabId());
        startReorder(mInteractingTab);

        // Verify
        assertTrue("Should have end margin.", mStripTab3.getTrailingMargin() > 0);
    }

    private void doTestHover_overIndividualTab_sameTab() {
        // Start reorder to set interacting view
        startReorder(mInteractingTab);

        // Call - endX = end of interactingView
        updateReorderPosition(mInteractingTab.getDrawX() + TAB_WIDTH);

        // Verify - only one set of animations run (for startReorder)
        verify(mAnimationHost, times(1)).startAnimations(anyList(), isNull());

        // Verify - no change to interacting view
        verifyViewHovered(mInteractingTab);
        verifyViewNotHovered(mStripTab1);
        verifyViewNotHovered(mStripTab2);
        verifyViewNotHovered(mStripTab3);
    }

    @Test
    public void testHoverTab_overIndividualTab_sameTab() {
        doTestHover_overIndividualTab_sameTab();
    }

    @Test
    public void testHoverGroup_overIndividualTab_sameTab() {
        setupDragDropState(/* isGroupDrag= */ true);
        doTestHover_overIndividualTab_sameTab();
    }

    private void doTestHover_overIndividualTab_differentTab() {
        // Start reorder to set interacting view
        startReorder(mInteractingTab);

        // Move drag to mStripTab2
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        updateReorderPosition(
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin());

        // Verify - two animations run (one for startReorder and one for successful updateReorder)
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        // Verify hover state updated
        verifyViewNotHovered(mInteractingTab);
        verifyViewHovered(mStripTab2);
    }

    @Test
    public void testHoverTab_overIndividualTab_differentTab() {
        doTestHover_overIndividualTab_differentTab();
    }

    @Test
    public void testHoverGroup_overIndividualTab_differentTab() {
        setupDragDropState(/* isGroupDrag= */ true);
        doTestHover_overIndividualTab_differentTab();
    }

    private void doTestHover_overCollapsedGroup() {
        // Set up tab group metadata. Group and collapse tabs.
        mockTabInGroup(INTERACTING_VIEW_ID);
        mInteractingGroupTitle.setCollapsed(true);

        // Start reorder on group title and verify initial state
        startReorder(mInteractingGroupTitle);
        verifyCollapsedGroupTitleHovered();

        // Move drag to the end of mInteractingTabGroupTitle (accounting for the trailing margin).
        // Verify end state.
        updateReorderPosition(
                mInteractingGroupTitle.getDrawX()
                        + TAB_WIDTH
                        + mInteractingGroupTitle.getTrailingMargin());
        verifyCollapsedGroupTitleHovered();

        // Verify - only one set of animations run (for startReorder)
        verify(mAnimationHost, times(1)).startAnimations(anyList(), isNull());
    }

    @Test
    public void testHoverTab_overCollapsedGroup() {
        doTestHover_overCollapsedGroup();
    }

    @Test
    public void testHoverGroup_overCollapsedGroup() {
        setupDragDropState(/* isGroupDrag= */ true);
        doTestHover_overCollapsedGroup();
    }

    private void doTestHoverTab_overExpandedGroup(StripLayoutView hoveredView, int numTabsInGroup) {
        float initialBottomIndicatorWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        mInteractingGroupTitle, numTabsInGroup, EFFECTIVE_TAB_WIDTH);
        mInteractingGroupTitle.setBottomIndicatorWidth(initialBottomIndicatorWidth);

        // Start reorder to set interacting view
        startReorder(mInteractingTab);

        // Move drag to group indicator
        // Call - endX = end of mInteractingGroupTitle
        updateReorderPosition(hoveredView.getDrawX() + TAB_WIDTH);

        // Verify
        verifyViewHovered(hoveredView);
        assertEquals(
                "Bottom indicator width should now account for trailing margin.",
                initialBottomIndicatorWidth + hoveredView.getTrailingMargin(),
                mInteractingGroupTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testHoverTab_overExpandedGroup_overGroupIndicator() {
        mockTabInGroup(mStripTab2.getTabId());
        doTestHoverTab_overExpandedGroup(mInteractingGroupTitle, /* numTabsInGroup= */ 1);
    }

    @Test
    public void testHoverTab_overExpandedGroup_overGroupedTab() {
        mockTabInGroup(mStripTab2.getTabId());
        doTestHoverTab_overExpandedGroup(mStripTab2, /* numTabsInGroup= */ 1);
    }

    @Test
    public void testHoverGroup_overExpandedGroup() {
        // Set up tab group metadata.
        setupDragDropState(/* isGroupDrag= */ true);

        // Group tabs.
        mockTabGroup(
                GROUP_ID1,
                INTERACTING_VIEW_ID,
                mModel.getTabById(INTERACTING_VIEW_ID),
                mModel.getTabById(TAB_ID2));
        float initialBottomIndicatorWidth = mInteractingGroupTitle.getBottomIndicatorWidth();

        // Start reorder to set interacting view - bottom indicator width should be 0 for
        // non-trailing tab in group.
        startReorder(mInteractingGroupTitle);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());
        assertTrue(
                "Interacting tab trailing margin should be 0 in non-trailing tab in group",
                mInteractingTab.getTrailingMargin() == 0);
        assertEquals(
                "Bottom indicator width should not change if there is no trailing margin",
                initialBottomIndicatorWidth,
                mInteractingGroupTitle.getBottomIndicatorWidth(),
                EPSILON);

        // Move drag to mStripTab2.
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        updateReorderPosition(
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin());

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        assertEquals(
                "mStripTab2 should become interacting view",
                mStripTab2,
                mStrategy.getInteractingView());
        // Verify trailing margins updated.
        assertTrue(
                "mStripTab2 should have trailing margin set", mStripTab2.getTrailingMargin() > 0);
        assertEquals(
                "mStripTab2 bottom indicator width should not change for group hover",
                initialBottomIndicatorWidth,
                mInteractingGroupTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testUpdateReorder_hoveredInStartGap() {
        // Start reorder to set interacting view - interacting view gets trailing margin
        startReorder(mInteractingTab);
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingTab.getTrailingMargin() > 0);

        // Call - endX = start gap
        updateReorderPosition(0);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        // Verify trailing margins updated
        verifyViewNotHovered(mInteractingTab);
        assertNull("Interacting view should not be set", mStrategy.getInteractingView());
    }

    @Test
    public void testStopReorder() {
        // Start reorder to set interacting view - interacting view gets trailing margin
        startReorder(mInteractingTab);

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify
        verify(mAnimationHost, times(2)).startAnimations(anyList(), any());
        assertNull("Interacting view should be null", mStrategy.getInteractingView());
        assertEquals(
                "mInteractingViewDuringStop should be set",
                mInteractingTab,
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    @Test
    public void testHandleDrop() {
        // Mock interacting view in group
        mockTabInGroup(INTERACTING_VIEW_ID);

        // Start and stop reorder to set interacting view on stop.
        startReorder(mInteractingTab);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);
        assertEquals(
                "mInteractingViewDuringStop should be set",
                mInteractingTab,
                mStrategy.getInteractingViewDuringStopForTesting());

        // Call
        List<Integer> list = new ArrayList<>(Arrays.asList(TAB_ID1, TAB_ID2));
        int dropIndex = 3;
        mStrategy.handleDrop(mGroupTitles, list, dropIndex);

        // Verify
        Tab expectedPrimaryTab = mModel.getTabByIdChecked(INTERACTING_VIEW_ID);
        int expectedMergeIndex = 0;
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        mTabListCaptor.capture(),
                        eq(expectedPrimaryTab),
                        eq(expectedMergeIndex),
                        eq(MergeNotificationType.DONT_NOTIFY));
        List<Tab> mergedTabs = mTabListCaptor.getValue();
        assertEquals("Unexpected number of tabs.", list.size(), mergedTabs.size());
        for (Tab tab : mergedTabs) {
            assertTrue("Unexpected merged tab.", list.contains(tab.getId()));
        }
        verify(mAnimationHost, times(3)).startAnimations(anyList(), any());
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    @Test
    public void testHandleDrop_hoveredTabNotInGroup_noOp() {
        // Start and stop reorder at mStripTab1 (not in group) to set interacting view on stop.
        startReorder(mStripTab1);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Call
        int draggedTabId = 100; // Arbitrary value.
        int dropIndex = 1;
        mStrategy.handleDrop(mGroupTitles, Collections.singletonList(draggedTabId), dropIndex);

        // Verify
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    // ============================================================================================
    // Event helpers
    // ============================================================================================

    private void startReorder(StripLayoutView interactingView) {
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, interactingView, DRAG_START_POINT);
    }

    private void updateReorderPosition(float endX) {
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                endX,
                /* deltaX= */ 0,
                ReorderType.DRAG_ONTO_STRIP);
    }

    // ============================================================================================
    // Verification helpers
    // ============================================================================================

    private void verifyViewHovered(StripLayoutView stripView) {
        assertEquals(
                "Hovered view should be interacting view.",
                stripView,
                mStrategy.getInteractingView());
        assertTrue("View should have trailing margin set.", stripView.getTrailingMargin() > 0);
    }

    private void verifyViewNotHovered(StripLayoutView stripView) {
        assertNotEquals(
                "Non-hovered view should not be interacting view.",
                stripView,
                mStrategy.getInteractingView());
        assertEquals(
                "View should not have trailing margin set.",
                0,
                stripView.getTrailingMargin(),
                EPSILON);
    }

    private void verifyCollapsedGroupTitleHovered() {
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());
        assertEquals(
                "mInteractingTabGroupTitle should be the interacting view.",
                mInteractingGroupTitle,
                mStrategy.getInteractingView());
        assertTrue(
                "mInteractingTabGroupTitle trailing margin should be set.",
                mInteractingGroupTitle.getTrailingMargin() > 0);
        assertTrue(
                "Collapsed group title bottom indicator width should be 0.",
                mInteractingGroupTitle.getBottomIndicatorWidth() == 0);
    }

    // ============================================================================================
    // Mock helpers
    // ============================================================================================

    private void mockTabInGroup(int id) {
        Tab tab = mModel.getTabById(id);
        mockTabGroup(GROUP_ID1, tab.getId(), tab);
    }

    private void setupDragDropState(boolean isGroupDrag) {
        ChromeDropDataAndroid dropData;
        if (isGroupDrag) {
            TabGroupMetadata tabGroupMetadata =
                    new TabGroupMetadata(
                            /* selectedTabId= */ INTERACTING_VIEW_ID,
                            /* sourceWindowId= */ 1,
                            /* tabGroupId= */ new Token(2L, 2L),
                            /* tabIdsToUrls= */ new ArrayList<>(),
                            /* tabGroupColor= */ 0,
                            /* tabGroupTitle= */ "Collaboration Group",
                            /* mhtmlTabTitle= */ null,
                            /* tabGroupCollapsed= */ false,
                            /* isGroupShared= */ false,
                            /* isIncognito= */ false);
            dropData =
                    new ChromeTabGroupDropDataAndroid.Builder()
                            .withTabGroupMetadata(tabGroupMetadata)
                            .build();
        } else {
            Tab tab = mModel.getTabById(INTERACTING_VIEW_ID);
            dropData = new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        }
        TrackerToken dragTrackerToken =
                DragDropGlobalState.store(
                        /* dragSourceInstanceId= */ 1, dropData, /* dragShadowBuilder= */ null);
        TabDragHandlerBase.setDragTrackerTokenForTesting(dragTrackerToken);
    }
}
