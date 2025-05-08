// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.AnimatorListenerAdapter;
import android.view.HapticFeedbackConstants;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link GroupReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class GroupReorderStrategyTest extends ReorderStrategyTestBase {
    // Constants
    private static final float DELTA = 0.f;

    // tab reorder threshold = (width(50) - overlap(28)) * constant(0.53) = 11.66
    private static final float DRAG_PAST_TAB_FAIL = 10.f;
    private static final float DRAG_PAST_TAB_SUCCESS = 20.f;
    // collapsed group reorder threshold = width(50) * constant(0.53) = 26.5
    private static final float DRAG_PAST_COLLAPSED_GROUP_FAIL = 20.f;
    private static final float DRAG_PAST_COLLAPSED_GROUP_SUCCESS = 40.f;
    // expanded group reorder threshold = width(100) * constant(0.53) = 53
    private static final float DRAG_PAST_EXPANDED_GROUP_FAIL = 40.f;
    private static final float DRAG_PAST_EXPANDED_GROUP_SUCCESS = 80.f;

    // Data = [Tab1]  [Group1]([Tab2])  [Group2]([Tab3])
    private StripLayoutGroupTitle mGroupTitle1;
    private StripLayoutGroupTitle mGroupTitle2;
    private StripLayoutTab mStripTab1;
    private StripLayoutTab mStripTab2;
    private StripLayoutTab mStripTab3;

    private StripLayoutView[] mExpandedGroup;
    private StripLayoutView[] mCollapsedGroup;

    // Target
    private GroupReorderStrategy mStrategy;
    private StripLayoutView[] mDraggedGroup;

    @Before
    @Override
    public void setup() {
        super.setup();
        mockTabGroup(GROUP_ID1, TAB_ID2, mModel.getTabById(TAB_ID2));
        mockTabGroup(GROUP_ID2, TAB_ID3, mModel.getTabById(TAB_ID3));

        mStrategy =
                new GroupReorderStrategy(
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
    }

    /**
     * Mocks the {@link StripLayoutView}s. One ungrouped tab, one tab in an expanded group, and one
     * tab in a collapsed group.
     */
    @Override
    protected void setupStripViews() {
        // [Tab1]  [Group1]([Tab2])  [Group2]([Tab3])
        mStripTab1 = buildStripTab(TAB_ID1, /* x= */ 0);
        mGroupTitle1 = buildGroupTitle(TAB_ID2, GROUP_ID1, TAB_WIDTH);
        mStripTab2 = buildStripTab(TAB_ID2, 2 * TAB_WIDTH);
        mGroupTitle2 = buildGroupTitle(TAB_ID3, GROUP_ID2, 3 * TAB_WIDTH);
        mStripTab3 = buildStripTab(TAB_ID3, 4 * TAB_WIDTH);

        // Construct expanded group.
        mExpandedGroup = new StripLayoutView[] {mGroupTitle1, mStripTab2};
        mGroupTitle1.setBottomIndicatorWidth(2 * TAB_WIDTH);

        // Construct collapsed group.
        mCollapsedGroup = new StripLayoutView[] {mGroupTitle2, mStripTab3};
        for (StripLayoutView view : mCollapsedGroup) {
            view.setCollapsed(/* collapsed= */ true);
        }

        // Populate state.
        mStripTabs = new StripLayoutTab[] {mStripTab1, mStripTab2, mStripTab3};
        mGroupTitles = new StripLayoutGroupTitle[] {mGroupTitle1, mGroupTitle2};
        mStripViews =
                new StripLayoutView[] {
                    mStripTab1, mGroupTitle1, mStripTab2, mGroupTitle2, mStripTab3
                };
    }

    @Test
    public void testStartReorder() {
        // Drag the expanded group
        mModel.setIndex(/* i= */ 1, TabSelectionType.FROM_USER);
        startReorder(mExpandedGroup);

        // Verify
        StripLayoutView interactingView = mStrategy.getInteractingView();
        assertEquals("Unexpected interacting view.", mInteractingGroupTitle, interactingView);

        List<StripLayoutView> draggedGroupList = Arrays.asList(mDraggedGroup);
        for (StripLayoutView view : mStripViews) {
            assertEquals(
                    "Only grouped views should be foregrounded.",
                    draggedGroupList.contains(view),
                    view.isForegrounded());
        }

        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNull());
        verify(mContainerView).performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
    }

    /**
     * Starts a reorder and drags a given distance. Verifies a reorder is successfully triggered.
     *
     * @param draggedGroup The list of {@link StripLayoutView}s in the dragged group.
     * @param rebuildDeltaX Signed value that represents how much the views' {@code idealX} (would)
     *     change once the strip rebuilds after a reorder. See {@link #mockRebuildForViews}.
     * @param dragDeltaX Signed value that represents how far to drag the group.
     * @param expectedIndex The index that we expect to reorder the group to after the drag.
     */
    private void testUpdateReorder_success(
            StripLayoutView[] draggedGroup,
            float rebuildDeltaX,
            float dragDeltaX,
            int expectedIndex) {
        mockRebuildForViews(draggedGroup, rebuildDeltaX);
        startReorderAndDragGroup(draggedGroup, dragDeltaX);

        float expectedOffset = dragDeltaX - rebuildDeltaX;
        verifySuccessfulDrag(expectedIndex, expectedOffset);
    }

    @Test
    public void testUpdateReorder_success_pastTab() {
        //    <------------
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_success(
                mExpandedGroup, -TAB_WIDTH, -DRAG_PAST_TAB_SUCCESS, /* expectedIndex= */ 0);
    }

    @Test
    public void testUpdateReorder_success_pastCollapsedGroup() {
        //                ------------------>
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_success(
                mExpandedGroup,
                TAB_WIDTH,
                DRAG_PAST_COLLAPSED_GROUP_SUCCESS,
                /* expectedIndex= */ 3);
    }

    @Test
    public void testUpdateReorder_success_pastExpandedGroup() {
        //                <------------------
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_success(
                mCollapsedGroup,
                -(2 * TAB_WIDTH),
                -DRAG_PAST_EXPANDED_GROUP_SUCCESS,
                /* expectedIndex= */ 1);
    }

    /**
     * Starts a reorder and drags a given distance. Verifies a reorder is not triggered.
     *
     * @param draggedGroup The list of {@link StripLayoutView}s in the dragged group.
     * @param dragDeltaX Signed value that represents how far to drag the group.
     */
    private void testUpdateReorder_fail(StripLayoutView[] draggedGroup, float dragDeltaX) {
        startReorderAndDragGroup(draggedGroup, dragDeltaX);
        verifyFailedDrag(dragDeltaX);
    }

    @Test
    public void testUpdateReorder_fail_pastTab() {
        //    <------------
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_fail(mExpandedGroup, -DRAG_PAST_TAB_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_pastCollapsedGroup() {
        //                ------------------>
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_fail(mExpandedGroup, DRAG_PAST_COLLAPSED_GROUP_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_pastExpandedGroup() {
        //                <------------------
        // [Tab1]  [ExpandedGroup]  [CollapsedGroup]
        testUpdateReorder_fail(mCollapsedGroup, -DRAG_PAST_EXPANDED_GROUP_FAIL);
    }

    @Test
    public void testStopReorder() {
        startReorder(mExpandedGroup);
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost, times(1))
                .startAnimations(anyList(), any(AnimatorListenerAdapter.class));
        assertNull("Should clear interacting view on stop.", mStrategy.getInteractingView());
    }

    // ============================================================================================
    // Event helpers
    // ============================================================================================

    private void startReorder(StripLayoutView[] draggedGroup) {
        mDraggedGroup = draggedGroup;
        mInteractingGroupTitle = (StripLayoutGroupTitle) draggedGroup[0];
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingGroupTitle, DRAG_START_POINT);
    }

    private void startReorderAndDragGroup(StripLayoutView[] draggedGroup, float deltaX) {
        startReorder(draggedGroup);
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mInteractingGroupTitle.getDrawX() + deltaX,
                deltaX,
                ReorderType.DRAG_WITHIN_STRIP);
    }

    // ============================================================================================
    // Verification helpers
    // ============================================================================================

    private void verifySuccessfulDrag(int expectedIndex, float expectedOffset) {
        verify(mTabGroupModelFilter)
                .moveRelatedTabs(mInteractingGroupTitle.getRootId(), expectedIndex);
        verify(mAnimationHost).startAnimations(anyList(), isNull());

        for (StripLayoutView view : mDraggedGroup) {
            assertEquals("Unexpected offset.", expectedOffset, view.getOffsetX(), DELTA);
        }
    }

    private void verifyFailedDrag(float expectedOffset) {
        verify(mTabGroupModelFilter, never())
                .moveRelatedTabs(eq(mInteractingGroupTitle.getRootId()), anyInt());
        verify(mAnimationHost, never()).startAnimations(anyList(), isNull());

        for (StripLayoutView view : mDraggedGroup) {
            assertEquals("Unexpected offset.", expectedOffset, view.getOffsetX(), DELTA);
        }
    }

    // ============================================================================================
    // Mock helpers
    // ============================================================================================

    /**
     * Updates {@code mStripTabs} and the {@code idealX} for the dragged {@link StripLayoutView}s in
     * response to a {@link TabGroupModelFilter#moveRelatedTabs}. This "fakes" a tab strip rebuild.
     */
    private void mockRebuildForViews(StripLayoutView[] draggedGroup, float deltaFromNewPosition) {
        doAnswer(
                        invocation -> {
                            for (StripLayoutView view : draggedGroup) {
                                view.setIdealX(view.getIdealX() + deltaFromNewPosition);
                            }
                            int id = invocation.getArgument(0);
                            int index = Math.min(mStripTabs.length - 1, invocation.getArgument(1));
                            mStripTabs[index] = StripLayoutUtils.findTabById(mStripTabs, id);
                            return null;
                        })
                .when(mTabGroupModelFilter)
                .moveRelatedTabs(anyInt(), anyInt());
    }
}
