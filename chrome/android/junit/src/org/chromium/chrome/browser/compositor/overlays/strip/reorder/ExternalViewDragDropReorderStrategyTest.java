// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.floatThat;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
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
        mModel.addTab(
                mTabForInteractingView,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
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
        mStripTab1 = buildStripTab(TAB_ID1, 0);
        mInteractingGroupTitle = buildGroupTitle(INTERACTING_VIEW_ID, GROUP_ID1, TAB_WIDTH);
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
        // Mock interacting view in a group
        when(mTabForInteractingView.getTabGroupId()).thenReturn(GROUP_ID1);
        when(mTabGroupModelFilter.getTabCountForGroup(GROUP_ID1)).thenReturn(2);

        // Call
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Verify - animations
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());

        // Verify - No edge margins, interacting view has trailing margin updated.
        verifyNoInteractions(mScrollDelegate);
        assertEquals(
                "Second to last tab should not have trailing margin",
                0,
                mStripTab2.getTrailingMargin(),
                EPSILON);
        assertEquals(
                "Last tab should not have trailing margin",
                0,
                mStripTab3.getTrailingMargin(),
                EPSILON);
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingTab.getTrailingMargin() > 0);
        assertTrue(
                "Interacting view title should have bottom indicator width set",
                mInteractingGroupTitle.getBottomIndicatorWidth() > 0);
    }

    @Test
    public void testStartReorder_firstAndLastTabInGroup_setEdgeMargins() {
        mockTabInGroup(mStripTab1.getTabId());
        mockTabInGroup(mStripTab3.getTabId());

        // Call
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNull());

        verify(mScrollDelegate).setReorderStartMargin(floatThat(f -> f > 0f));
        assertEquals(
                "Second to last tab should not have trailing margin",
                0,
                mStripTab2.getTrailingMargin(),
                EPSILON);
        assertTrue("Last tab should have trailing margin", mStripTab3.getTrailingMargin() > 0);
    }

    @Test
    public void testUpdateReorder_hoveredTabSameAsInteractingView_noOp() {
        // Start reorder to set interacting view
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Call - endX = end of interactingView
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mInteractingTab.getDrawX() + TAB_WIDTH,
                0,
                ReorderType.DRAG_ONTO_STRIP);

        // Verify - no change to interacting view.
        assertEquals(
                "Interacting view should not change",
                mInteractingTab,
                mStrategy.getInteractingView());
    }

    @Test
    public void testHoverOverCollapsedGroup_groupTitleHasTrailingMargin() {
        // Set up tab group metadata.
        setupDragDropState(/* isGroupDrag= */ true);

        // Group and collapse tabs.
        mockTabInGroup(INTERACTING_VIEW_ID);
        mockTabInGroup(mStripTab2.getTabId());
        mInteractingGroupTitle.setCollapsed(true);

        // Start reorder to set interacting view - bottom indicator width should be 0 if collapsed.
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());
        assertTrue(
                "Interacting view trailing margin should be set",
                mInteractingGroupTitle.getTrailingMargin() > 0);
        assertTrue(
                "Collapsed group title bottom indicator width should be 0",
                mInteractingGroupTitle.getBottomIndicatorWidth() == 0);

        // Move drag to mInteractingTabGroupTitle.
        // Call - endX = end of mInteractingGroupTitle (accounting for interacting view's trailing
        // margin)
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mInteractingGroupTitle.getDrawX()
                        + TAB_WIDTH
                        + mInteractingGroupTitle.getTrailingMargin(),
                0,
                ReorderType.DRAG_ONTO_STRIP);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(1)).startAnimations(anyList(), isNull());

        assertEquals(
                "mInteractingTabGroupTitle should become interacting view",
                mInteractingGroupTitle,
                mStrategy.getInteractingView());
        // Verify trailing margins updated.
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingGroupTitle.getTrailingMargin() > 0);
        assertTrue(
                "Collapsed group title bottom indicator width should be 0",
                mInteractingGroupTitle.getBottomIndicatorWidth() == 0);
    }

    @Test
    public void testHoverGroupOverLastTabInGroup_hasTrailingMargin_noBottomIndicator() {
        // Set up tab group metadata.
        setupDragDropState(/* isGroupDrag= */ true);

        // Group and collapse tabs.
        mockTabInGroup(INTERACTING_VIEW_ID);
        mockTabInGroup(mStripTab2.getTabId());
        float initialBottomIndicatorWidth = mInteractingGroupTitle.getBottomIndicatorWidth();

        // Start reorder to set interacting view - bottom indicator width should be 0 for
        // non-trailing tab in group.
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), any());
        assertTrue(
                "Interacting tab trailing margin should be 0 in non-trailing tab in group",
                mInteractingTab.getTrailingMargin() == 0);
        assertTrue(
                "Bottom indicator width should not change if there is no trailing margin",
                mInteractingGroupTitle.getBottomIndicatorWidth() - initialBottomIndicatorWidth
                        == 0);

        // Move drag to mStripTab2.
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin(),
                0,
                ReorderType.DRAG_ONTO_STRIP);

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
        assertTrue(
                "mStripTab2 bottom indicator width should not change for group hover",
                mInteractingGroupTitle.getBottomIndicatorWidth() - initialBottomIndicatorWidth
                        == 0);
    }

    @Test
    public void testHoverGroupOverIndividualTab_hasTrailingMargin() {
        // Set up tab group metadata.
        setupDragDropState(/* isGroupDrag= */ true);

        // Start reorder to set interacting view
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        assertTrue(
                "Interacting tab trailing margin should be set",
                mInteractingTab.getTrailingMargin() > 0);

        // Move drag to mStripTab2.
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin(),
                0,
                ReorderType.DRAG_ONTO_STRIP);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        assertEquals(
                "mStripTab2 should become interacting view",
                mStripTab2,
                mStrategy.getInteractingView());
        // Verify trailing margins updated
        assertTrue(
                "Old interacting view trailing margin should be 0",
                mInteractingTab.getTrailingMargin() == 0);
        assertTrue(
                "mStripTab2 should have trailing margin set", mStripTab2.getTrailingMargin() > 0);
    }

    @Test
    public void testUpdateReorder_hoveredTabDiffThanInteractingView_updateInteractingView() {
        mockTabInGroup(mStripTab2.getTabId());
        // Start reorder to set interacting view - interacting view gets trailing margin
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingTab.getTrailingMargin() > 0);

        // Move drag to mStripTab2
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin(),
                0,
                ReorderType.DRAG_ONTO_STRIP);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        assertEquals(
                "mStripTab2 should become interacting view",
                mStripTab2,
                mStrategy.getInteractingView());
        // Verify trailing margins updated
        assertTrue(
                "Interacting view should not have trailing margin set",
                mInteractingTab.getTrailingMargin() == 0);
        assertTrue(
                "mStripTab2 should have trailing margin set", mStripTab2.getTrailingMargin() > 0);
    }

    @Test
    public void testUpdateReorder_hoveredTabCollapsed_noBottomIndicator() {
        // Group and collapse tabs.
        mockTabInGroup(INTERACTING_VIEW_ID);
        mockTabInGroup(mStripTab2.getTabId());
        mInteractingGroupTitle.setCollapsed(true);

        // Start reorder to set interacting view - interacting view shouldn't bottom indicator width
        // if collapsed.
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingTab.getTrailingMargin() > 0);
        assertTrue(
                "Collapsed group title bottom indicator width should be 0",
                mInteractingGroupTitle.getBottomIndicatorWidth() == 0);

        // Move drag to mStripTab2
        // Call - endX = end of mStripTab2 (accounting for interacting view's trailing margin)
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mStripTab2.getDrawX() + TAB_WIDTH + mInteractingTab.getTrailingMargin(),
                0,
                ReorderType.DRAG_ONTO_STRIP);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        assertEquals(
                "mStripTab2 should become interacting view",
                mStripTab2,
                mStrategy.getInteractingView());
        // Verify trailing margins updated
        assertTrue(
                "Interacting view should not have trailing margin set",
                mInteractingTab.getTrailingMargin() == 0);
        assertTrue(
                "Interacting view should have trailing margin set",
                mStripTab2.getTrailingMargin() > 0);
        assertTrue(
                "Collapsed group title bottom indicator width should be 0",
                mInteractingGroupTitle.getBottomIndicatorWidth() == 0);
    }

    @Test
    public void testUpdateReorder_hoveredInStartGap() {
        // Start reorder to set interacting view - interacting view gets trailing margin
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        assertTrue(
                "Interacting view should have trailing margin set",
                mInteractingTab.getTrailingMargin() > 0);

        // Call - endX = start gap
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, 0, 0, ReorderType.DRAG_ONTO_STRIP);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());

        // Verify trailing margins updated
        assertTrue(
                "Interacting view should not have trailing margin set",
                mInteractingTab.getTrailingMargin() == 0);
        assertNull("Interacting view should not be set", mStrategy.getInteractingView());
    }

    @Test
    public void testStopReorder() {
        // Start reorder to set interacting view - interacting view gets trailing margin
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

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
        when(mTabForInteractingView.getTabGroupId()).thenReturn(GROUP_ID1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(true);

        // Start and stop reorder to set interacting view on stop.
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);
        assertEquals(
                "mInteractingViewDuringStop should be set",
                mInteractingTab,
                mStrategy.getInteractingViewDuringStopForTesting());

        // Call
        List<Integer> list = new ArrayList<>(Arrays.asList(100, 101, 102)); // Arbitrary values.
        int dropIndex = 1;
        mStrategy.handleDrop(mGroupTitles, list, dropIndex);

        // Verify
        for (Integer tabId : list) {
            verify(mTabGroupModelFilter).mergeTabsToGroup(tabId, INTERACTING_VIEW_ID, true);
            verify(mModel).moveTab(tabId, dropIndex);
            verify(mAnimationHost, times(3)).startAnimations(anyList(), any());
        }
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    @Test
    public void testHandleDrop_hoveredTabNotInGroup_noOp() {
        // Start and stop reorder at mStripTab1 (not in group) to set interacting view on stop.
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mStripTab1, DRAG_START_POINT);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Call
        int draggedTabId = 100; // Arbitrary value.
        int dropIndex = 1;
        mStrategy.handleDrop(mGroupTitles, Collections.singletonList(draggedTabId), dropIndex);

        // Verify
        verify(mTabGroupModelFilter, times(0))
                .mergeTabsToGroup(draggedTabId, INTERACTING_VIEW_ID, true);
        verify(mModel, times(0)).moveTab(draggedTabId, dropIndex);
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    private void mockTabInGroup(int id) {
        Tab tab = mModel.getTabById(id);
        mockTabGroup(GROUP_ID1, tab.getId(), tab);
    }

    private void setupDragDropState(boolean isGroupDrag) {
        ChromeDropDataAndroid dropData;
        if (isGroupDrag) {
            TabGroupMetadata tabGroupMetadata =
                    new TabGroupMetadata(
                            /* rootId= */ mTabForInteractingView.getId(),
                            /* selectedTabId= */ mTabForInteractingView.getId(),
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
            dropData =
                    new ChromeTabDropDataAndroid.Builder().withTab(mTabForInteractingView).build();
        }
        TrackerToken dragTrackerToken =
                DragDropGlobalState.store(
                        /* dragSourceInstanceId= */ 1, dropData, /* dragShadowBuilder= */ null);
        TabDragSource.setDragTrackerTokenForTesting(dragTrackerToken);
    }
}
