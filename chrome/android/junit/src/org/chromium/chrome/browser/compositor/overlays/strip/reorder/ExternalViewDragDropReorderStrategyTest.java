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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.tab.Tab;

/** Tests for {@link ExternalViewDragDropReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class ExternalViewDragDropReorderStrategyTest extends ReorderStrategyTestBase {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Constants
    private static final Integer INTERACTING_VIEW_ROOT_ID = 24; // Arbitrary value.

    // Data
    private StripLayoutTab mStripTab1;
    private StripLayoutTab mStripTab2;
    private StripLayoutTab mStripTab3;
    @Mock private Tab mTabForStripTab1;
    @Mock private Tab mTabForStripTab2;
    @Mock private Tab mTabForStripTab3;

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
                        mTabWidthSupplier);
        when(mTabWidthSupplier.get()).thenReturn((float) TAB_WIDTH);
        setupStripViews();
    }

    @Test
    public void testStartReorder() {
        // Mock interacting view in a group
        when(mTabForInteractingView.getRootId()).thenReturn(INTERACTING_VIEW_ROOT_ID);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(INTERACTING_VIEW_ROOT_ID))
                .thenReturn(2);

        // Call
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

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
                mInteractingTabGroupTitle.getBottomIndicatorWidth() > 0);
    }

    @Test
    public void testStartReorder_firstAndLastTabInGroup_setEdgeMargins() {
        mockTabInGroup(mStripTab1.getTabId(), mTabForStripTab1);
        mockTabInGroup(mStripTab3.getTabId(), mTabForStripTab3);

        // Call
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

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
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

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
    public void testUpdateReorder_hoveredTabDiffThanInteractingView_updateInteractingView() {
        mockTabInGroup(mStripTab2.getTabId(), mTabForStripTab2);
        // Start reorder to set interacting view - interacting view gets trailing margin
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
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
    public void testUpdateReorder_hoveredInStartGap() {
        // Start reorder to set interacting view - interacting view gets trailing margin
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
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
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Call
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);

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
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(true);
        when(mTabForInteractingView.getRootId()).thenReturn(INTERACTING_VIEW_ROOT_ID);

        // Start and stop reorder to set interacting view on stop.
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);
        assertEquals(
                "mInteractingViewDuringStop should be set",
                mInteractingTab,
                mStrategy.getInteractingViewDuringStopForTesting());

        // Call
        int draggedTabId = 100;
        int dropIndex = 1;
        mStrategy.handleDrop(mGroupTitles, draggedTabId, dropIndex);

        // Verify
        verify(mTabGroupModelFilter).mergeTabsToGroup(draggedTabId, INTERACTING_VIEW_ID, true);
        verify(mModel).moveTab(draggedTabId, dropIndex);
        verify(mAnimationHost, times(3)).startAnimations(anyList(), any());
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    @Test
    public void testHandleDrop_hoveredTabNotInGroup_noOp() {
        // Start and stop reorder at mStripTab1 (not in group) to set interacting view on stop.
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mStripTab1, DRAG_START_POINT);
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);

        // Call
        int draggedTabId = 100; // Arbitrary value.
        int dropIndex = 1;
        mStrategy.handleDrop(mGroupTitles, draggedTabId, dropIndex);

        // Verify
        verify(mTabGroupModelFilter, times(0))
                .mergeTabsToGroup(draggedTabId, INTERACTING_VIEW_ID, true);
        verify(mModel, times(0)).moveTab(draggedTabId, dropIndex);
        assertNull(
                "mInteractingViewDuringStop should be null",
                mStrategy.getInteractingViewDuringStopForTesting());
    }

    private void mockTabInGroup(int id, Tab tabForStripTab) {
        when(mModel.getTabById(id)).thenReturn(tabForStripTab);
        when(mTabGroupModelFilter.isTabInTabGroup(tabForStripTab)).thenReturn(true);
    }

    private void setupStripViews() {
        mStripTab1 = buildStripTab(1, 0, TAB_WIDTH);
        mInteractingTabGroupTitle =
                buildGroupTitle(INTERACTING_VIEW_ROOT_ID, GROUP_ID, TAB_WIDTH, TAB_WIDTH);
        mInteractingTab = buildStripTab(INTERACTING_VIEW_ID, 2 * TAB_WIDTH, TAB_WIDTH);
        mStripTab2 = buildStripTab(2, 3 * TAB_WIDTH, TAB_WIDTH);
        mStripTab3 = buildStripTab(3, 4 * TAB_WIDTH, TAB_WIDTH);

        mStripTabs = new StripLayoutTab[] {mStripTab1, mInteractingTab, mStripTab2, mStripTab3};
        mGroupTitles = new StripLayoutGroupTitle[] {mInteractingTabGroupTitle};
        mStripViews =
                new StripLayoutView[] {
                    mStripTab1, mInteractingTabGroupTitle, mInteractingTab, mStripTab2, mStripTab3
                };
    }
}
