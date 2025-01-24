// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.graphics.PointF;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ReorderDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.TabDragSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;

import java.util.Collections;

/** Tests for {@link SourceViewDragDropReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class SourceViewDragDropReorderStrategyTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final PointF DRAG_START_POINT = new PointF(70f, 20f);
    private static final float END_X = 10f;
    private static final float DELTA_X = 5f;
    private static final float EPSILON = 0.001f;

    // Dependencies
    @Mock private TabDragSource mTabDragSource;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private ReorderStrategy mTabStrategy;
    @Mock private AnimationHost mAnimationHost;
    @Mock private StripUpdateDelegate mStripUpdateDelegate;
    @Mock private ScrollDelegate mScrollDelegate;
    @Mock private View mContainerView;
    @Mock private ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mModel;
    @Mock private ReorderDelegate mReorderDelegate;
    @Mock private Supplier<Float> mTabWidthSupplier;
    @Mock private TabUngrouper mTabUnGrouper;

    // Data
    private StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    private StripLayoutGroupTitle[] mGroupTitles = new StripLayoutGroupTitle[0];
    private StripLayoutView[] mStripViews = new StripLayoutView[0];
    @Mock private StripLayoutTab mInteractingView;
    @Mock private Tab mTabForInteractingView;

    // Target
    private SourceViewDragDropReorderStrategy mStrategy;

    @Before
    public void setup() {
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
                        mTabDragSource,
                        mActionConfirmationManager,
                        mTabStrategy);
        when(mModel.getTabById(anyInt())).thenReturn(mTabForInteractingView);
    }

    @Test
    public void testStartReorder_dragStarted() {
        when(mTabDragSource.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);

        // Call
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingView, DRAG_START_POINT);

        // Assert
        assertNotNull("Dragged view should not be null", mStrategy.getViewBeingDraggedForTesting());
    }

    @Test
    public void testStartReorder_dragFailed_fallback() {
        when(mTabDragSource.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(false);

        // Call
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingView, DRAG_START_POINT);

        // Assert
        assertNull("Dragged view should be null", mStrategy.getViewBeingDraggedForTesting());

        // Verify fallback
        verify(mReorderDelegate).stopReorderMode(mGroupTitles, mStripTabs);
        verify(mReorderDelegate)
                .startReorderMode(
                        mStripTabs,
                        mGroupTitles,
                        mInteractingView,
                        DRAG_START_POINT,
                        ReorderType.DRAG_WITHIN_STRIP);
    }

    @Test
    public void testUpdateReorder_dragOntoStrip() {
        startReorder();

        // Call
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

        // Verify tab properties
        verify(mInteractingView).setIsDraggedOffStrip(false);
        verify(mInteractingView).setOffsetX(0f);
        verify(mInteractingView).setOffsetY(0f);

        // Verify
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(false, null, false);
        verify(mTabStrategy)
                .startReorderMode(
                        eq(mStripTabs),
                        eq(mGroupTitles),
                        eq(mInteractingView),
                        eq(new PointF(END_X, 0f)));
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragWithinStrip() {
        startReorder();

        // Call
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
        // Tab not in group - no prompt shown.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);
        // Set properties for dragged tab.
        float drawX = 24f; // Arbitrary value.
        float height = 35f; // Arbitrary value.
        when(mInteractingView.getIdealX()).thenReturn(drawX);
        when(mInteractingView.getHeight()).thenReturn(height);
        startReorder();

        // Call
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);

        // Verify tab properties
        verify(mInteractingView).setIsDraggedOffStrip(true);
        verify(mInteractingView).setDrawX(drawX);
        verify(mInteractingView).setDrawY(height);
        verify(mInteractingView).setOffsetY(height);

        // Verify
        verify(mTabStrategy).stopReorderMode(mGroupTitles, mStripTabs);
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingView, false);
        verifyNoMoreInteractions(mTabStrategy);
    }

    @Test
    public void testUpdateReorder_dragOutOfAndThenOntoStrip() {
        // Tab not in group - no prompt shown.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);
        startReorder();

        // Update reorder - drag out of strip to set lastOffsetX
        float lastOffsetX = 12f; // Arbitrary value.
        when(mInteractingView.getOffsetX()).thenReturn(lastOffsetX);
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);

        // Call - drag onto strip.
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

        // Verify tab offsetX
        verify(mInteractingView).setOffsetX(lastOffsetX);
    }

    @Test
    public void testUpdateReorder_dragOutOfStripWithPrompt() {
        // Last tab in group. Will not skip ungrouping.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUnGrouper);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(true);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(1);
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(false);
        startReorder();

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
        startReorder();
        mStrategy.updateReorderPosition(
                mStripViews, mGroupTitles, mStripTabs, END_X, DELTA_X, ReorderType.DRAG_ONTO_STRIP);

        // Call
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify
        verify(mTabStrategy).stopReorderMode(mGroupTitles, mStripTabs);
    }

    @Test
    public void testStopReorder_withTabRestore() {
        // No prompt for update - drag out of strip.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);

        // Start reorder. Simulate drag off strip.
        startReorder();
        when(mInteractingView.isDraggedOffStrip()).thenReturn(true);

        // Call
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);

        // Verify restore.
        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mInteractingView).setIsDraggedOffStrip(false);
        verify(mInteractingView).setWidth(0.f);
        verify(mStripUpdateDelegate).resizeTabStrip(true, mInteractingView, true);
    }

    @Test
    public void testStopReorder_afterDragOutOfStrip_tabStrategyStopInvokedOnce() {
        // No prompt for update - drag out of strip.
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mTabForInteractingView)).thenReturn(false);

        // Start and update reorder - drag out of strip. Verify tab strategy stop invoked.
        startReorder();
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                END_X,
                DELTA_X,
                ReorderType.DRAG_OUT_OF_STRIP);
        verify(mTabStrategy).stopReorderMode(mGroupTitles, mStripTabs);

        // Call - Stop drag and drop strategy.
        mStrategy.stopReorderMode(mGroupTitles, mStripTabs);

        // Assert
        assertNull("View being dragged should be null", mStrategy.getViewBeingDraggedForTesting());
        assertEquals(0f, mStrategy.getDragLastOffsetXForTesting(), EPSILON);

        // Verify - tab strategy stop is not invoked again.
        verifyNoMoreInteractions(mTabStrategy);
    }

    private void startReorder() {
        when(mTabDragSource.startTabDragAction(
                        Mockito.eq(mContainerView),
                        eq(mTabForInteractingView),
                        eq(DRAG_START_POINT),
                        anyFloat(),
                        anyFloat()))
                .thenReturn(true);
        mStrategy.startReorderMode(mStripTabs, mGroupTitles, mInteractingView, DRAG_START_POINT);
    }
}
