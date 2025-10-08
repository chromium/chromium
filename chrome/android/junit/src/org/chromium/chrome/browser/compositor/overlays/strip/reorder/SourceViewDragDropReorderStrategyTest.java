// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.graphics.PointF;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;

import java.util.Collections;
import java.util.List;

/** Tests for {@link SourceViewDragDropReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class SourceViewDragDropReorderStrategyTest extends ReorderStrategyTestBase {
    // Constants
    private static final float END_X = 10f;
    private static final float DELTA_X = 5f;

    // Dependencies
    @Mock private TabStripDragHandler mTabStripDragHandler;
    @Mock protected ReorderStrategy mTabStrategy;
    @Mock protected ReorderStrategy mMultiTabStrategy;
    @Mock protected ReorderStrategy mGroupStrategy;

    // Data
    private MockTab mTabForInteractingView;
    private StripLayoutTab mOtherSelectedTab;

    // Target
    private SourceViewDragDropReorderStrategy mStrategy;

    @Before
    @Override
    public void setup() {
        super.setup();
        mStrategy =
                new SourceViewDragDropReorderStrategy(
                        mReorderDelegate,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier,
                        mLastReorderScrollTimeSupplier,
                        mTabStripDragHandler,
                        mActionConfirmationManager,
                        mTabStrategy,
                        mMultiTabStrategy,
                        mGroupStrategy);
    }

    @Override
    protected void setupStripViews() {}

    private void setupForTabDrag() {
        mInteractingTab = buildStripTab(TAB_ID1, 0);
        mTabForInteractingView = (MockTab) mModel.getTabById(TAB_ID1);

        when(mTabStrategy.getInteractingView()).thenReturn(mInteractingTab);
    }

    private void setupForMultiTabDrag() {
        mInteractingTab = buildStripTab(TAB_ID1, 0);
        mTabForInteractingView = (MockTab) mModel.getTabById(TAB_ID1);
        mOtherSelectedTab = buildStripTab(TAB_ID2, 1);
        mStripTabs = new StripLayoutTab[] {mInteractingTab, mOtherSelectedTab};

        when(mModel.isTabMultiSelected(TAB_ID1)).thenReturn(true);
        when(mModel.isTabMultiSelected(TAB_ID2)).thenReturn(true);
        when(mModel.getMultiSelectedTabsCount()).thenReturn(2);
    }

    private void setupForGroupDrag() {
        mInteractingTab = buildStripTab(TAB_ID1, 0);
        mTabForInteractingView = (MockTab) mModel.getTabById(TAB_ID1);
        mInteractingGroupTitle = buildGroupTitle(GROUP_ID1, TAB_WIDTH);
        mStripTabs =
                new StripLayoutTab[] {
                    buildStripTab(TAB_ID1, 0), buildStripTab(TAB_ID2, 0), buildStripTab(TAB_ID3, 0)
                };

        mockTabGroup(GROUP_ID1, TAB_ID1, mModel.getTabById(TAB_ID1));
    }

    @Test
    public void testStartReorder_tabDragStarted() {
        // Call
        startTabReorder();

        // Verify
        assertNotNull("Dragged view should not be null", mStrategy.getViewBeingDraggedForTesting());
        verify(mTabStripDragHandler)
                .startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    public void testStartReorder_multiTabDragStarted() {
        // Call
        startMultiTabReorder();

        // Verify
        assertNotNull("Dragged view should not be null", mStrategy.getViewBeingDraggedForTesting());
        verify(mTabStripDragHandler)
                .startMultiTabDragAction(
                        eq(mContainerView),
                        any(List.class),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    public void testStartReorder_tabDragFailed_fallback() {
        setupForTabDrag();
        when(mTabStripDragHandler.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(false);

        // Call
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Verify fallback
        verify(mReorderDelegate).stopReorderMode(mStripViews, mGroupTitles);
        verify(mReorderDelegate)
                .startReorderMode(
                        mStripViews,
                        mStripTabs,
                        mGroupTitles,
                        mInteractingTab,
                        DRAG_START_POINT,
                        ReorderType.DRAG_WITHIN_STRIP);
    }

    @Test
    public void testStartReorder_groupDrag() {
        // Call
        startGroupReorder();

        // Verify
        verify(mTabStripDragHandler)
                .startGroupDragAction(
                        mContainerView,
                        GROUP_ID1,
                        /* isGroupShared= */ false,
                        DRAG_START_POINT,
                        TAB_WIDTH,
                        TAB_WIDTH);
    }

    @Test
    public void testUpdateReorder_dragOntoStrip() {
        // Call
        startTabReorder();

        // Verify tab properties
        assertFalse("DraggedOffStrip should be false", mInteractingTab.isDraggedOffStrip());
        assertEquals("OffsetX should be 0", 0f, mInteractingTab.getOffsetX(), EPSILON);
        assertEquals("OffsetY should be 0", 0f, mInteractingTab.getOffsetY(), EPSILON);

        // Verify
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(false);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(false, null, false);
        verify(mTabStrategy)
                .startReorderMode(
                        eq(mStripViews),
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(mInteractingTab),
                        eq(new PointF(END_X, 0f)));
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_multiTab_dragOntoStrip() {
        // Call
        startMultiTabReorder();

        // Verify tab properties
        assertFalse("DraggedOffStrip should be false", mInteractingTab.isDraggedOffStrip());
        assertFalse("DraggedOffStrip should be false", mOtherSelectedTab.isDraggedOffStrip());
        assertEquals("OffsetX should be 0", 0f, mInteractingTab.getOffsetX(), EPSILON);
        assertEquals("OffsetY should be 0", 0f, mInteractingTab.getOffsetY(), EPSILON);
        assertEquals("OffsetX should be 0", 0f, mOtherSelectedTab.getOffsetX(), EPSILON);
        assertEquals("OffsetY should be 0", 0f, mOtherSelectedTab.getOffsetY(), EPSILON);

        // Verify
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(false);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(false, null, false);
        verify(mMultiTabStrategy)
                .startReorderMode(
                        eq(mStripViews),
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(mInteractingTab),
                        eq(new PointF(END_X, 0f)));
        verifyNoMoreInteractions(mMultiTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragWithinStrip() {
        // Start reorder before dragging within strip.
        startTabReorder();
        verify(mTabStrategy)
                .startReorderMode(
                        eq(mStripViews),
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(mInteractingTab),
                        eq(new PointF(END_X, 0f)));

        // Call - drag within strip.
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_WITHIN_STRIP);

        // Verify
        verify(mTabStrategy)
                .updateReorderPosition(
                        mStripViews,
                        mGroupTitles,
                        mStripTabs,
                        END_X,
                        DELTA_X,
                        ReorderType.DRAG_WITHIN_STRIP);
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_multiTab_dragWithinStrip() {
        // Start reorder before dragging within strip.
        startMultiTabReorder();
        verify(mMultiTabStrategy)
                .startReorderMode(
                        eq(mStripViews),
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(mInteractingTab),
                        eq(new PointF(END_X, 0f)));

        // Call - drag within strip.
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_WITHIN_STRIP);

        // Verify
        verify(mMultiTabStrategy)
                .updateReorderPosition(
                        mStripViews,
                        mGroupTitles,
                        mStripTabs,
                        END_X,
                        DELTA_X,
                        ReorderType.DRAG_WITHIN_STRIP);
        verifyNoMoreInteractions(mMultiTabStrategy);
    }

    @Test
    public void testUpdateReorder_multiTab_dragOutOfStrip() {
        startMultiTabReorder();
        // Set properties for dragged tab.
        float drawX = 24f; // Arbitrary value.
        mInteractingTab.setIdealX(drawX);

        // Call
        dragOutOfStrip();
        verifyDragOutOfStrip(mMultiTabStrategy);

        // Verify tab properties
        assertTrue("DraggedOffStrip should be true", mInteractingTab.isDraggedOffStrip());
        assertTrue("DraggedOffStrip should be true", mOtherSelectedTab.isDraggedOffStrip());
        assertEquals(
                "DrawY should match height",
                mInteractingTab.getHeight(),
                mInteractingTab.getDrawY(),
                EPSILON);
        assertEquals(
                "OffsetY should match height",
                mInteractingTab.getHeight(),
                mInteractingTab.getOffsetY(),
                EPSILON);
        assertEquals(
                "DrawY should match height",
                mOtherSelectedTab.getHeight(),
                mOtherSelectedTab.getDrawY(),
                EPSILON);
        assertEquals(
                "OffsetY should match height",
                mOtherSelectedTab.getHeight(),
                mOtherSelectedTab.getOffsetY(),
                EPSILON);

        // Verify
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(true);
        verify(mMultiTabStrategy).stopReorderMode(mStripViews, mGroupTitles);
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate, times(2)).resizeTabStrip(false, null, false);
        verifyNoMoreInteractions(mMultiTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragOutOfStripWithNoPrompt() {
        startTabReorder();
        // Set properties for dragged tab.
        float drawX = 24f; // Arbitrary value.
        mInteractingTab.setIdealX(drawX);

        // Call
        dragOutOfStrip();
        verifyDragOutOfStrip(mTabStrategy);

        // Verify tab properties
        assertTrue("DraggedOffStrip should be true", mInteractingTab.isDraggedOffStrip());
        assertEquals(
                "DrawX should match idealX",
                mInteractingTab.getIdealX(),
                mInteractingTab.getDrawX(),
                EPSILON);
        assertEquals(
                "DrawY should match height",
                mInteractingTab.getHeight(),
                mInteractingTab.getDrawY(),
                EPSILON);
        assertEquals(
                "OffsetY should match height",
                mInteractingTab.getHeight(),
                mInteractingTab.getOffsetY(),
                EPSILON);

        // Verify
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(true);
        verify(mTabStrategy).stopReorderMode(mStripViews, mGroupTitles);
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingTab, false);
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip() {
        startTabReorder();

        // Update reorder - drag out of strip to set lastOffsetX
        float lastOffsetX = 12f; // Arbitrary value.
        mInteractingTab.setOffsetX(lastOffsetX);
        dragOutOfStrip();
        verifyDragOutOfStrip(mTabStrategy);
        assertEquals(
                "LastOffsetX should be set",
                lastOffsetX,
                mStrategy.getDragLastOffsetXForTesting(),
                EPSILON);

        // Call - drag onto strip.
        dragOntoStrip();
        assertEquals(
                "LastOffsetX should be unset",
                0,
                mStrategy.getDragLastOffsetXForTesting(),
                EPSILON);

        // Verify tab offsetX
        assertEquals("OffsetX should be set", lastOffsetX, mInteractingTab.getOffsetX(), EPSILON);

        // Verify compositor buttons hidden and then shown
        verify(mStripUpdateDelegate, times(2)).setCompositorButtonsVisible(false);
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(true);
    }

    private void doTestDragOutOfAndThenOntoStripSelection() {
        int startingIndex = mModel.index();

        // Update reorder - drag out of strip and fake next selection index.
        int expectedIndex = 2;
        when(mStripUpdateDelegate.getNextIndexAfterClose(any())).thenReturn(expectedIndex);
        dragOutOfStrip();
        assertEquals(
                "Expected to select the next available index on drag exit.",
                expectedIndex,
                mModel.index());

        // Stop reorder - verify the original index is re-selected.
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);
        assertEquals(
                "Expected to reselect the initial selected tab on drag enter.",
                startingIndex,
                mModel.index());
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip_tabSelection() {
        startTabReorder();
        doTestDragOutOfAndThenOntoStripSelection();
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip_multiTabSelection() {
        startMultiTabReorder();
        doTestDragOutOfAndThenOntoStripSelection();
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip_groupSelection() {
        startGroupReorder();
        doTestDragOutOfAndThenOntoStripSelection();
    }

    @Test
    public void testUpdateReorder_dragOutOfStripWithPrompt() {
        startTabReorder();
        // Last tab in group. Will not skip ungrouping.
        mockTabGroup(GROUP_ID1, TAB_ID1, mTabForInteractingView);
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(false);

        // Call
        dragOutOfStrip();
        verifyAdditionalCallsForTabSelection(mTabStrategy);

        // Verify
        verify(mTabUnGrouper)
                .ungroupTabs(
                        eq(Collections.singletonList(mTabForInteractingView)),
                        eq(false),
                        eq(true),
                        any(TabModelActionListener.class));
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testStopReorder_withoutTabRestore() {
        // Start and update reorder - drag onto strip.
        startTabReorder();

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify
        verify(mTabStrategy).stopReorderMode(mStripViews, mGroupTitles);
    }

    @Test
    public void testStopReorder_withTabRestore() {
        // Start reorder. Simulate drag off strip.
        startTabReorder();
        mInteractingTab.setIsDraggedOffStrip(true);

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify restore.
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingTab, true);
        assertFalse("DraggedOffStrip should be false", mInteractingTab.isDraggedOffStrip());
        assertEquals("Width should be 0", 0f, mInteractingTab.getWidth(), EPSILON);
    }

    @Test
    public void testStopReorder_withoutGroupRestore() {
        // Start and update reorder - drag onto strip.
        startGroupReorder();

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify
        verify(mGroupStrategy).stopReorderMode(mStripViews, mGroupTitles);
    }

    @Test
    public void testStopReorder_withGroupRestore() {
        // Start reorder. Simulate drag off strip.
        startGroupReorder();
        mInteractingGroupTitle.setIsDraggedOffStrip(true);

        // Call. Simulate failed drop.
        when(mTabGroupModelFilter.tabGroupExists(GROUP_ID1)).thenReturn(true);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify restore.
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate, times(2)).resizeTabStrip(false, null, false);
        assertFalse("DraggedOffStrip should be false", mInteractingGroupTitle.isDraggedOffStrip());
        assertEquals("offsetY should be 0", 0f, mInteractingGroupTitle.getOffsetY(), EPSILON);
    }

    @Test
    public void testStopReorder_afterDragOutOfStrip_tabStrategyStopInvokedOnce() {
        // Start and update reorder - drag out of strip. Verify tab strategy stop invoked.
        startTabReorder();
        dragOutOfStrip();
        verifyDragOutOfStrip(mTabStrategy);

        // Call - Stop drag and drop strategy.
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify - tab strategy stop is not invoked again.
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testStopReorder_multiTab_withRestore() {
        // Start reorder. Simulate drag off strip.
        startMultiTabReorder();
        mInteractingTab.setIsDraggedOffStrip(true);
        mOtherSelectedTab.setIsDraggedOffStrip(true);

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify restore.
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate, times(2)).resizeTabStrip(false, null, false);
        assertFalse("DraggedOffStrip should be false", mInteractingTab.isDraggedOffStrip());
        assertFalse("DraggedOffStrip should be false", mOtherSelectedTab.isDraggedOffStrip());
        assertEquals("offsetY should be 0", 0f, mInteractingTab.getOffsetY(), EPSILON);
        assertEquals("offsetY should be 0", 0f, mOtherSelectedTab.getOffsetY(), EPSILON);
    }

    private void dragOntoStrip() {
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);
    }

    private void verifyDragOntoStrip(
            ReorderStrategy reorderStrategy, StripLayoutView interactingView) {
        verify(reorderStrategy)
                .startReorderMode(
                        eq(mStripViews),
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(interactingView),
                        eq(new PointF(END_X, 0f)));
    }

    private void dragOutOfStrip() {
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);
    }

    private void verifyDragOutOfStrip(ReorderStrategy reorderStrategy) {
        verify(reorderStrategy).stopReorderMode(mStripViews, mGroupTitles);
        verifyAdditionalCallsForTabSelection(reorderStrategy);
    }

    private void verifyAdditionalCallsForTabSelection(ReorderStrategy reorderStrategy) {
        if (reorderStrategy == mTabStrategy) {
            verify(reorderStrategy).getInteractingView();
        }
    }

    private void startTabReorder() {
        setupForTabDrag();
        when(mTabStripDragHandler.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        dragOntoStrip();
        verifyDragOntoStrip(mTabStrategy, mInteractingTab);
    }

    private void startMultiTabReorder() {
        setupForMultiTabDrag();
        when(mTabStripDragHandler.startMultiTabDragAction(
                        any(), any(), any(), any(), anyFloat(), anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        dragOntoStrip();
        verifyDragOntoStrip(mMultiTabStrategy, mInteractingTab);
    }

    private void startGroupReorder() {
        setupForGroupDrag();
        when(mTabStripDragHandler.startGroupDragAction(
                        Mockito.eq(mContainerView),
                        eq(GROUP_ID1),
                        anyBoolean(),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);
        dragOntoStrip();
        verifyDragOntoStrip(mGroupStrategy, mInteractingGroupTitle);
    }
}
