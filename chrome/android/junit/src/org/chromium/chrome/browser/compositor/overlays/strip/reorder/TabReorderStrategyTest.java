// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.HapticFeedbackConstants;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/** Tests for {@link TabReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class TabReorderStrategyTest extends ReorderStrategyTestBase {
    @Rule
    @SuppressWarnings("HidingField")
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Constants
    private static final float DELTA = 0.f;

    // tab reorder threshold = (width(50) - overlap(28)) * constant(0.53) = 11.66
    private static final float DRAG_PAST_TAB_FAIL = 10.f;
    private static final float DRAG_PAST_TAB_SUCCESS = 15.f;

    // pinned tab reorder threshold = (width(108) - overlap(28)) * constant(0.53) = 42.4
    private static final float DRAG_PAST_PINNED_TAB_SUCCESS = 43.f;

    // collapsed group reorder threshold = width(50) * constant(0.53) = 26.5
    private static final float DRAG_PAST_COLLAPSED_GROUP_FAIL = 20.f;
    private static final float DRAG_PAST_COLLAPSED_GROUP_SUCCESS = 30.f;
    // drag-in threshold = (width(50) - overlap(28) / 2) * constant(0.53) = 5.83
    private static final float DRAG_INTO_GROUP_FAIL = 5.f;
    private static final float DRAG_INTO_GROUP_SUCCESS = 6.f;
    // drag-in threshold = (width(50) - overlap(28) / 2) * constant(0.53) = 5.83
    private static final float DRAG_OUT_OF_GROUP_FAIL = 3.f;
    private static final float DRAG_OUT_OF_GROUP_SUCCESS = 8.f;
    // drag-in threshold = (width(50) - overlap(28) / 2) * constant(0.53) + width(50) = 55.83
    private static final float DRAG_OUT_OF_GROUP_PAST_INDICATOR_FAIL = 50.f;
    private static final float DRAG_OUT_OF_GROUP_PAST_INDICATOR_SUCCESS = 60.f;

    private StripLayoutTab mCollapsedTab;
    private StripLayoutTab mUngroupedTab1;
    private StripLayoutTab mUngroupedTab2;
    private StripLayoutTab mExpandedTab1;
    private StripLayoutTab mExpandedTab2;
    private StripLayoutTab mLastTab;

    private StripLayoutGroupTitle mExpandedTitle;

    // Dependencies
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>();

    // Target
    private TabReorderStrategy mStrategy;

    @Before
    @Override
    public void setup() {
        super.setup();
        mockTabGroup(GROUP_ID1, TAB_ID1, mModel.getTabById(TAB_ID1));
        mockTabGroup(GROUP_ID2, TAB_ID4, mModel.getTabById(TAB_ID4), mModel.getTabById(TAB_ID5));

        mInReorderModeSupplier.set(false);
        mStrategy =
                new TabReorderStrategy(
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
                        mInReorderModeSupplier);
    }

    /**
     * Mocks the {@link StripLayoutView}s. One ungrouped tab, one tab in an expanded group, and one
     * tab in a collapsed group.
     */
    @Override
    protected void setupStripViews() {
        // Data = [CollapsedGroup]([Tab])  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        StripLayoutGroupTitle groupTitle1 = buildGroupTitle(GROUP_ID1, /* x= */ 0);
        mCollapsedTab = buildStripTab(TAB_ID1, TAB_WIDTH);
        mUngroupedTab1 = buildStripTab(TAB_ID2, 2 * TAB_WIDTH);
        mUngroupedTab2 = buildStripTab(TAB_ID3, 3 * TAB_WIDTH);
        mExpandedTitle = buildGroupTitle(GROUP_ID2, 4 * TAB_WIDTH);
        mExpandedTab1 = buildStripTab(TAB_ID4, 5 * TAB_WIDTH);
        mExpandedTab2 = buildStripTab(TAB_ID5, 6 * TAB_WIDTH);
        mLastTab = buildStripTab(TAB_ID6, 7 * TAB_WIDTH);

        // Collapse views in collapsed group.
        groupTitle1.setCollapsed(/* collapsed= */ true);
        mCollapsedTab.setCollapsed(/* collapsed= */ true);

        // Set indicator width for expanded group.
        mExpandedTitle.setBottomIndicatorWidth(3 * TAB_WIDTH);

        // Populate state.
        mStripTabs =
                new StripLayoutTab[] {
                    mCollapsedTab,
                    mUngroupedTab1,
                    mUngroupedTab2,
                    mExpandedTab1,
                    mExpandedTab2,
                    mLastTab
                };
        mGroupTitles = new StripLayoutGroupTitle[] {groupTitle1, mExpandedTitle};
        mStripViews =
                new StripLayoutView[] {
                    groupTitle1,
                    mCollapsedTab,
                    mUngroupedTab1,
                    mUngroupedTab2,
                    mExpandedTitle,
                    mExpandedTab1,
                    mExpandedTab2,
                    mLastTab
                };
    }

    @Test
    public void testStartReorder() {
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]  [Tab]
        // Mock the last tab as grouped, so both edge tabs are grouped (and edge margins are set).
        int tabId = mLastTab.getTabId();
        mockTabGroup(GROUP_ID3, tabId, mModel.getTabById(tabId));

        // Start reordering the second ungrouped tab.
        startReorder(mUngroupedTab2);

        // Verify
        assertEquals(
                "Unexpected interacting view.", mInteractingTab, mStrategy.getInteractingView());
        assertTrue("Interacting tab should be foregrounded.", mInteractingTab.isForegrounded());

        String message = "Unexpected group margin.";
        float expectedMargin =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier, /* isPinned= */ false)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        verify(mScrollDelegate).setReorderStartMargin(expectedMargin);
        assertEquals(message, 0, mCollapsedTab.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mUngroupedTab1.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mUngroupedTab2.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mExpandedTab1.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mExpandedTab2.getTrailingMargin(), DELTA);
        assertEquals(message, expectedMargin, mLastTab.getTrailingMargin(), DELTA);

        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNull());
        verify(mContainerView).performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
    }

    /**
     * Starts a reorder and drags a given distance. Verifies a reorder is successfully triggered.
     *
     * @param draggedTab The dragged {@link StripLayoutTab}.
     * @param dragDeltaX Signed value that represents how far to drag the tab.
     * @param expectedOffset The expected end offsetX for the dragged tab.
     */
    private void testUpdateReorder_success(
            StripLayoutTab draggedTab, float dragDeltaX, float expectedOffset) {
        startReorderAndDragTab(draggedTab, dragDeltaX);
        verifySuccessfulDrag(expectedOffset);
    }

    /**
     * Starts a reorder and drags a given distance. Verifies a reorder is successfully triggered.
     *
     * @param draggedTab The dragged {@link StripLayoutTab}.
     * @param rebuildDeltaX Signed value that represents how much the tab's {@code idealX} (would)
     *     change once the strip rebuilds after a reorder. See {@link #mockRebuildForDraggedTab}.
     * @param dragDeltaX Signed value that represents how far to drag the tab.
     * @param expectedIndex The index that we expect to reorder the tab to after the drag.
     */
    private void testUpdateReorder_success(
            StripLayoutTab draggedTab, float rebuildDeltaX, float dragDeltaX, int expectedIndex) {
        mockRebuildForDraggedTab(draggedTab, rebuildDeltaX);

        float expectedOffset = dragDeltaX - rebuildDeltaX;
        testUpdateReorder_success(draggedTab, dragDeltaX, expectedOffset);

        verify(mModel).moveTab(mInteractingTab.getTabId(), expectedIndex);
    }

    @Test
    @Feature("Pinned Tabs")
    public void testUpdateReorder_success_pinnedTabs() {
        // Pinned tabs should live at strip start, however, this test and below only checks the
        // success/failure of the reorder, the initial position doesn't matter, so reuse the current
        // StripViews here for now. Should consider to refactor this setup for clarity.

        //                     -------->
        // [CollapsedGroup]  [PinnedTab]  [PinnedTab]  [ExpandedGroup]  [Tab]
        mUngroupedTab1.setIsPinned(true);
        mUngroupedTab2.setIsPinned(true);
        Tab tab1 = mModel.getTabAt(1);
        Tab tab2 = mModel.getTabAt(2);
        tab1.setIsPinned(true);
        tab2.setIsPinned(true);
        testUpdateReorder_success(
                mUngroupedTab1, TAB_WIDTH, DRAG_PAST_PINNED_TAB_SUCCESS, /* expectedIndex= */ 2);
        verifyMoved();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP)
    public void testUpdateReorder_fail_pinnedTabs() {
        //                    -------->
        // [CollapsedGroup]  [PinnedTab]  [Tab]  [ExpandedGroup]  [Tab]
        mUngroupedTab1.setIsPinned(true);
        Tab tab1 = mModel.getTabAt(1);
        tab1.setIsPinned(true);

        // Though the drag threshold is reached, but pinned tab cannot trigger reorder for an
        // unpinned tab.
        testUpdateReorder_fail(mUngroupedTab1, DRAG_PAST_PINNED_TAB_SUCCESS);
    }

    @Test
    public void testUpdateReorder_success_pastTab_ungrouped() {
        //                     -------->
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]  [Tab]
        testUpdateReorder_success(
                mUngroupedTab1, TAB_WIDTH, DRAG_PAST_TAB_SUCCESS, /* expectedIndex= */ 2);
        verifyMoved();
    }

    @Test
    public void testUpdateReorder_success_pastTab_grouped() {
        //                                                   <------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_success(
                mExpandedTab2, -TAB_WIDTH, -DRAG_PAST_TAB_SUCCESS, /* expectedIndex= */ 3);
        verifyMoved();
    }

    @Test
    public void testUpdateReorder_success_pastCollapsedGroup() {
        //       <--------------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]  [Tab]
        testUpdateReorder_success(
                mUngroupedTab1,
                -TAB_WIDTH,
                -DRAG_PAST_COLLAPSED_GROUP_SUCCESS,
                /* expectedIndex= */ 0);
        verifyMoved();
    }

    @Test
    public void testUpdateReorder_success_intoGroup() {
        //                                                           <------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_success(mLastTab, -DRAG_INTO_GROUP_SUCCESS, -DRAG_INTO_GROUP_SUCCESS);
        verifyMergedToGroup();
    }

    @Test
    public void testUpdateReorder_success_outOfGroup() {
        //                                                         ------>
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_success(
                mExpandedTab2, DRAG_OUT_OF_GROUP_SUCCESS, DRAG_OUT_OF_GROUP_SUCCESS);
        verifyUnGrouped();
    }

    @Test
    public void testUpdateReorder_success_outOfGroup_pastIndicator() {
        //                                     <--------------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_success(
                mExpandedTab1,
                -DRAG_OUT_OF_GROUP_PAST_INDICATOR_SUCCESS,
                -DRAG_OUT_OF_GROUP_PAST_INDICATOR_SUCCESS);
        verifyUnGrouped();
    }

    private void testUpdateReorder_fail(StripLayoutTab draggedTab, float dragDeltaX) {
        startReorderAndDragTab(draggedTab, dragDeltaX);
        verifyFailedDrag(dragDeltaX);
    }

    @Test
    public void testUpdateReorder_fail_pastTab_ungrouped() {
        //                     -------->
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]  [Tab]
        testUpdateReorder_fail(mUngroupedTab1, DRAG_PAST_TAB_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_pastTab_grouped() {
        //                                                   <------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_fail(mExpandedTab2, -DRAG_PAST_TAB_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_pastCollapsedGroup() {
        //       <--------------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]  [Tab]
        testUpdateReorder_fail(mUngroupedTab1, -DRAG_PAST_COLLAPSED_GROUP_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_intoGroup() {
        //                                                           <------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_fail(mLastTab, -DRAG_INTO_GROUP_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_outOfGroup() {
        //                                                         ------>
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_fail(mExpandedTab2, DRAG_OUT_OF_GROUP_FAIL);
    }

    @Test
    public void testUpdateReorder_fail_outOfGroup_pastIndicator() {
        //                                     <--------------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        testUpdateReorder_fail(mExpandedTab1, -DRAG_OUT_OF_GROUP_PAST_INDICATOR_FAIL);
    }

    @Test
    public void testUpdateReorder_bottomIndicatorWidth_mergeToGroup() {
        //                                                           <------
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        mockMergeToGroup();
        startReorderAndDragTab(mLastTab, -DRAG_INTO_GROUP_SUCCESS);

        int expectedNumTabs = 3;
        float expectedBottomIndicatorWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        mExpandedTitle, expectedNumTabs, EFFECTIVE_TAB_WIDTH);
        assertEquals(
                "Unexpected bottom indicator width.",
                expectedBottomIndicatorWidth,
                mExpandedTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testUpdateReorder_bottomIndicatorWidth_ungroup() {
        //                                                         ------>
        // [CollapsedGroup]  [Tab]  [Tab]  [ExpandedGroup]([Tab] [Tab])  [Tab]
        mockUnGroup();
        startReorderAndDragTab(mExpandedTab2, DRAG_OUT_OF_GROUP_SUCCESS);

        int expectedNumTabs = 1;
        float expectedBottomIndicatorWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        mExpandedTitle, expectedNumTabs, EFFECTIVE_TAB_WIDTH);
        assertEquals(
                "Unexpected bottom indicator width.",
                expectedBottomIndicatorWidth,
                mExpandedTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testStopReorder() {
        // Start reorder and verify.
        startReorder(mUngroupedTab2);
        verify(mAnimationHost).startAnimations(anyList(), isNull());

        // Stop reorder and verify.
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);
        verify(mScrollDelegate).setReorderStartMargin(0);
        assertEquals(
                "Should no longer have trailing margin.",
                0,
                mExpandedTab2.getTrailingMargin(),
                DELTA);
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNotNull());
    }

    // ============================================================================================
    // Event helpers
    // ============================================================================================

    private void startReorder(StripLayoutTab interactingTab) {
        mInteractingTab = interactingTab;
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        mInReorderModeSupplier.set(true);
    }

    private void startReorderAndDragTab(StripLayoutTab draggedTab, float deltaX) {
        startReorder(draggedTab);
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                draggedTab.getDrawX() + deltaX,
                deltaX,
                ReorderType.DRAG_WITHIN_STRIP);
    }

    // ============================================================================================
    // Verification helpers
    // ============================================================================================

    private void verifyMoved() {
        verify(mModel).moveTab(anyInt(), anyInt());
        verify(mTabUnGrouper, never()).ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt(), anyBoolean());
    }

    private void verifyUnGrouped() {
        verify(mModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabUnGrouper).ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt(), anyBoolean());
    }

    private void verifyMergedToGroup() {
        verify(mModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabUnGrouper, never()).ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(any(), any(), any(), anyInt());
    }

    private void verifySuccessfulDrag(float expectedOffset) {
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());
        assertEquals("Unexpected offset.", expectedOffset, mInteractingTab.getOffsetX(), DELTA);
    }

    private void verifyFailedDrag(float expectedOffset) {
        verify(mModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabUnGrouper, never()).ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), any(), anyInt());
        verify(mAnimationHost, times(1)).startAnimations(anyList(), isNull());
        assertEquals("Unexpected offset.", expectedOffset, mInteractingTab.getOffsetX(), DELTA);
    }

    // ============================================================================================
    // Mock helpers
    // ============================================================================================

    private void mockRebuildForDraggedTab(StripLayoutTab draggedTab, float deltaFromNewPosition) {
        doAnswer(
                        invocation -> {
                            draggedTab.setIdealX(draggedTab.getIdealX() + deltaFromNewPosition);
                            int id = invocation.getArgument(0);
                            int index = Math.min(mStripTabs.length - 1, invocation.getArgument(1));
                            mStripTabs[index] = StripLayoutUtils.findTabById(mStripTabs, id);
                            return null;
                        })
                .when(mModel)
                .moveTab(anyInt(), anyInt());
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void mockMergeToGroup() {
        doAnswer(
                        invocation -> {
                            Tab tab = mModel.getTabById(mTabCaptor.getValue().getId());
                            assertThat(tab).isNotNull();
                            Token tabGroupId = tab.getTabGroupId();
                            int count = mTabGroupModelFilter.getTabCountForGroup(tabGroupId);
                            when(mTabGroupModelFilter.getTabCountForGroup(tabGroupId))
                                    .thenReturn(count + 1);
                            return null;
                        })
                .when(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(any(), mTabCaptor.capture(), any(), anyInt());
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void mockUnGroup() {
        doAnswer(
                        invocation -> {
                            Tab tab = mTabListCaptor.getValue().get(0);
                            Token tabGroupId = tab.getTabGroupId();
                            int count = mTabGroupModelFilter.getTabCountForGroup(tabGroupId);
                            when(mTabGroupModelFilter.getTabCountForGroup(tabGroupId))
                                    .thenReturn(count - 1);
                            return null;
                        })
                .when(mTabUnGrouper)
                .ungroupTabs(mTabListCaptor.capture(), anyBoolean(), anyBoolean(), any());
    }
}
