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
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;

import java.util.Collections;

/** Tests for {@link SourceViewDragDropReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class SourceViewDragDropReorderStrategyTest extends ReorderStrategyTestBase {
    // Constants
    private static final Integer INTERACTING_VIEW_ROOT_ID = 24; // Arbitrary value.
    private static final float END_X = 10f; // Arbitrary value.
    private static final float DELTA_X = 5f; // Arbitrary value.

    // Dependencies
    @Mock private TabDragSource mTabDragSource;
    @Mock protected ReorderStrategy mTabStrategy;
    @Mock protected ReorderStrategy mGroupStrategy;

    // Target
    private SourceViewDragDropReorderStrategy mStrategy;

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
                        mTabDragSource,
                        mActionConfirmationManager,
                        mTabStrategy,
                        mGroupStrategy);
    }

    @Override
    protected void setupStripViews() {}

    private void setupForTabDrag() {
        mInteractingTab = buildStripTab(INTERACTING_VIEW_ID, 0);
    }

    private void setupForGroupDrag() {
        mInteractingGroupTitle = buildGroupTitle(INTERACTING_VIEW_ROOT_ID, GROUP_ID1, TAB_WIDTH);
    }

    @Test
    public void testStartReorder_tabDragStarted() {
        setupForTabDrag();
        when(mTabDragSource.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);

        // Call
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);

        // Assert
        assertNotNull("Dragged view should not be null", mStrategy.getViewBeingDraggedForTesting());

        // Verify
        verify(mTabDragSource)
                .startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    public void testStartReorder_tabDragFailed_fallback() {
        setupForTabDrag();
        when(mTabDragSource.startTabDragAction(
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
        setupForGroupDrag();

        // Call
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);

        // Verify
        verify(mTabDragSource)
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
        setupForTabDrag();
        startTabReorder();

        // Call
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

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
    public void testUpdateReorder_dragWithinStrip() {
        setupForTabDrag();
        startTabReorder();

        // Start reorder before dragging within strip.
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);
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
    public void testUpdateReorder_dragOutOfStripWithNoPrompt() {
        setupForTabDrag();
        // Tab not in group - no prompt shown.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);
        // Set properties for dragged tab.
        float drawX = 24f; // Arbitrary value.
        mInteractingTab.setIdealX(drawX);
        startTabReorder();

        // Call
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);

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
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingTab, false);
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip() {
        setupForTabDrag();
        // Tab not in group - no prompt shown.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);
        startTabReorder();

        // Update reorder - drag out of strip to set lastOffsetX
        float lastOffsetX = 12f; // Arbitrary value.
        mInteractingTab.setOffsetX(lastOffsetX);
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);
        assertEquals(
                "LastOffsetX should be set",
                lastOffsetX,
                mStrategy.getDragLastOffsetXForTesting(),
                EPSILON);

        // Call - drag onto strip.
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);
        assertEquals(
                "LastOffsetX should be unset",
                0,
                mStrategy.getDragLastOffsetXForTesting(),
                EPSILON);

        // Verify tab offsetX
        assertEquals("OffsetX should be set", lastOffsetX, mInteractingTab.getOffsetX(), EPSILON);

        // Verify compositor buttons hidden and then shown
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(false);
        verify(mStripUpdateDelegate).setCompositorButtonsVisible(true);
    }

    @Test
    public void testUpdateReorder_dragOutOfStripWithPrompt() {
        setupForTabDrag();
        // Last tab in group. Will not skip ungrouping.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(true);
        when(mTabGroupModelFilter.getTabCountForGroup(any())).thenReturn(1);
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(false);
        startTabReorder();

        // Call
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);

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
        setupForTabDrag();
        startTabReorder();
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

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
        setupForTabDrag();
        // No prompt for update - drag out of strip.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);

        // Start reorder. Simulate drag off strip.
        startTabReorder();
        mInteractingTab.setIsDraggedOffStrip(true);

        // Call
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify restore.
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingTab, true);
        assertFalse("DraggedOffStrip should be false", mInteractingTab.isDraggedOffStrip());
        assertEquals("Width should be 0", 0f, mInteractingTab.getWidth(), EPSILON);
    }

    @Test
    public void testStopReorder_withoutGroupRestore() {
        // Start and update reorder - drag onto strip.
        setupForGroupDrag();
        startGroupReorder();
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

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
        setupForGroupDrag();
        // Start reorder. Simulate drag off strip.
        startGroupReorder();
        mInteractingGroupTitle.setIsDraggedOffStrip(true);

        // Call. Simulate failed drop.
        when(mTabGroupModelFilter.tabGroupExists(GROUP_ID1)).thenReturn(true);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify restore.
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(false, null, false);
        assertFalse("DraggedOffStrip should be false", mInteractingGroupTitle.isDraggedOffStrip());
        assertEquals("offsetY should be 0", 0f, mInteractingGroupTitle.getOffsetY(), EPSILON);
    }

    @Test
    public void testStopReorder_afterDragOutOfStrip_tabStrategyStopInvokedOnce() {
        setupForTabDrag();
        // No prompt for update - drag out of strip.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);

        // Start and update reorder - drag out of strip. Verify tab strategy stop invoked.
        startTabReorder();
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);
        verify(mTabStrategy).stopReorderMode(mStripViews, mGroupTitles);

        // Call - Stop drag and drop strategy.
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify - tab strategy stop is not invoked again.
        verifyNoMoreInteractions(mTabStrategy);
    }

    private void startTabReorder() {
        when(mTabDragSource.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
    }

    private void startGroupReorder() {
        when(mTabDragSource.startGroupDragAction(
                        Mockito.eq(mContainerView),
                        eq(GROUP_ID1),
                        anyBoolean(),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);
    }
}
