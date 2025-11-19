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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** Tests for {@link MultiTabReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class MultiTabReorderStrategyTest extends ReorderStrategyTestBase {

    // Constants for drag thresholds. Values are arbitrary but chosen to distinguish
    // success/failure cases clearly.
    // tab reorder threshold = (width(50) - overlap(28)) * constant(0.53) = 11.66
    private static final float DRAG_PAST_TAB_FAIL = 10.f;
    private static final float DRAG_PAST_TAB_SUCCESS = 15.f;
    // collapsed group reorder threshold = width(50) * constant(0.53) = 26.5
    private static final float DRAG_PAST_COLLAPSED_GROUP_FAIL = 20.f;
    private static final float DRAG_PAST_COLLAPSED_GROUP_SUCCESS = 30.f;
    // drag-in threshold = (width(50) - overlap(28) / 2) * constant(0.53) = 5.83
    private static final float DRAG_INTO_GROUP_FAIL = 5.f;
    private static final float DRAG_INTO_GROUP_SUCCESS = 6.f;
    // drag-in threshold = (width(50) - overlap(28) / 2) * constant(0.53) = 5.83
    private static final float DRAG_OUT_OF_GROUP_FAIL = 3.f;
    private static final float DRAG_OUT_OF_GROUP_SUCCESS = 8.f;
    private static final float DELTA = 0.f;

    // View references
    private StripLayoutTab mUngroupedTab1;
    private StripLayoutTab mUngroupedTab2;
    private StripLayoutTab mUngroupedTab3;
    private StripLayoutTab mGroupedTab1;
    private StripLayoutTab mGroupedTab2;
    private StripLayoutTab mCollapsedGroupTab;

    private StripLayoutGroupTitle mExpandedGroupTitle;
    private StripLayoutGroupTitle mCollapsedGroupTitle;

    // Dependencies
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>();
    private final List<StripLayoutTab> mSelectedTabs = new ArrayList<>();
    private final List<Integer> mSelectedTabsIds = new ArrayList<>();
    private final List<StripLayoutTab> mUnpinnedTabs = new ArrayList<>();
    private final List<StripLayoutTab> mPinnedTabs = new ArrayList<>();

    // Target
    private MultiTabReorderStrategy mStrategy;
    @Mock protected Supplier<Float> mPinnedTabsBoundarySupplier;

    @Before
    @Override
    public void setup() {
        super.setup();
        mockTabGroup(GROUP_ID1, TAB_ID2, mModel.getTabById(TAB_ID2), mModel.getTabById(TAB_ID3));
        mockTabGroup(GROUP_ID2, TAB_ID4, mModel.getTabById(TAB_ID4));

        mInReorderModeSupplier.set(false);
        when(mModel.isTabMultiSelected(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int tabId = invocation.getArgument(0);
                            return mSelectedTabsIds.contains(tabId);
                        });

        when(mPinnedTabsBoundarySupplier.get()).thenReturn(0f);
        mStrategy =
                new MultiTabReorderStrategy(
                        mReorderDelegate,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier,
                        mPinnedTabsBoundarySupplier,
                        mLastReorderScrollTimeSupplier,
                        mInReorderModeSupplier);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUnGrouper);
    }

    @After
    public void tearDown() {
        mStrategy.clearReorderStateForTesting();
    }

    @Override
    protected void setupStripViews() {
        // Layout: [Tab1] [ExpandedGroup]([Tab2][Tab3]) [Tab4(CollapsedGroup)] [Tab5] [Tab6]
        mUngroupedTab1 = buildStripTab(TAB_ID1, 0);
        mExpandedGroupTitle = buildGroupTitle(GROUP_ID1, TAB_WIDTH);
        mGroupedTab1 = buildStripTab(TAB_ID2, 2 * TAB_WIDTH);
        mGroupedTab2 = buildStripTab(TAB_ID3, 3 * TAB_WIDTH);
        mCollapsedGroupTitle = buildGroupTitle(GROUP_ID2, 4 * TAB_WIDTH);
        mCollapsedGroupTab = buildStripTab(TAB_ID4, 4 * TAB_WIDTH);
        mUngroupedTab2 = buildStripTab(TAB_ID5, 5 * TAB_WIDTH);
        mUngroupedTab3 = buildStripTab(TAB_ID6, 6 * TAB_WIDTH);

        mExpandedGroupTitle.setBottomIndicatorWidth(2 * TAB_WIDTH);
        mCollapsedGroupTitle.setCollapsed(true);
        mCollapsedGroupTab.setCollapsed(true);

        mStripTabs =
                new StripLayoutTab[] {
                    mUngroupedTab1,
                    mGroupedTab1,
                    mGroupedTab2,
                    mCollapsedGroupTab,
                    mUngroupedTab2,
                    mUngroupedTab3
                };
        mGroupTitles = new StripLayoutGroupTitle[] {mExpandedGroupTitle, mCollapsedGroupTitle};
        mStripViews =
                new StripLayoutView[] {
                    mUngroupedTab1,
                    mExpandedGroupTitle,
                    mGroupedTab1,
                    mGroupedTab2,
                    mCollapsedGroupTitle,
                    mCollapsedGroupTab,
                    mUngroupedTab2,
                    mUngroupedTab3
                };
    }

    // startReorderMode tests
    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testStartReorder_gatherUngroupedTabs() {
        // Select two non-contiguous ungrouped tabs
        selectTabs(mUngroupedTab1, mUngroupedTab3);

        // Start reorder
        startReorder(mUngroupedTab1);

        verify(mModel, atLeastOnce()).setIndex(eq(0), anyInt());

        // Verify tabs were gathered. UngroupedTab3 (at model index 5) should move to index 1.
        verify(mModel).moveTab(mUngroupedTab3.getTabId(), 1);
        // Verify ungroup is not called.
        verify(mTabGroupModelFilter.getTabUngrouper(), times(1))
                .ungroupTabs(eq(Collections.emptyList()), anyBoolean(), anyBoolean(), any());
        // Verify tabs are foregrounded
        assertTrue("Selected tab should be foregrounded.", mUngroupedTab1.isForegrounded());
        assertTrue("Selected tab should be foregrounded.", mUngroupedTab3.isForegrounded());
        assertFalse("Unselected tab should not be foregrounded.", mUngroupedTab2.isForegrounded());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testStartReorder_mergeUngroupedTabOnGroupedPrimary() {
        // Select a grouped tab and an ungrouped tab
        selectTabs(mGroupedTab2, mUngroupedTab2);

        // Start reorder on the grouped tab
        startReorder(mGroupedTab2);

        verify(mModel).setIndex(eq(2), anyInt());

        // Verify the ungrouped tab is merged into the primary tab's group.
        Tab expectedPrimaryTab = mModel.getTabById(mGroupedTab2.getTabId());
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        mTabListCaptor.capture(), eq(expectedPrimaryTab), anyInt(), anyInt());
        assertEquals("Should merge 2 tabs.", 2, mTabListCaptor.getValue().size());

        // Verify no reorder operations took place.
        verify(mModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilter.getTabUngrouper(), never())
                .ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
        // Verify tabs are foregrounded.
        assertTrue("Selected tab should be foregrounded.", mGroupedTab2.isForegrounded());
        assertTrue("Selected tab should be foregrounded.", mUngroupedTab2.isForegrounded());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testStartReorder_ungroupAndGather_ungroupedPrimaryTab() {
        // Select a grouped tab and an ungrouped tab
        selectTabs(mGroupedTab1, mUngroupedTab2);

        // Start reorder on the grouped tab
        startReorder(mUngroupedTab2);

        verify(mModel).setIndex(eq(4), anyInt());

        // Verify ungroup is called for the selected tabs
        verify(mTabGroupModelFilter.getTabUngrouper())
                .ungroupTabs(mTabListCaptor.capture(), anyBoolean(), anyBoolean(), any());
        assertEquals("Should ungroup 1 tabs.", 1, mTabListCaptor.getValue().size());
        assertEquals(
                "Incorrect tab ungrouped.",
                mGroupedTab1.getTabId(),
                mTabListCaptor.getValue().get(0).getId());

        // Verify tabs are gathered. After ungrouping, mGroupedTab1 (model index 1) should move
        // next to mUngroupedTab2 (model index 4), so to index 4.
        verify(mModel).moveTab(mUngroupedTab2.getTabId(), 4);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    @SuppressWarnings("DirectInvocationOnMock")
    public void testStartReorder_nonPinnedPrimaryTab_pinnedTabMoveToLastPinnedPosition() {
        // Pin first two tabs.
        mUngroupedTab1.setIsPinned(true);
        mGroupedTab1.setIsPinned(true);
        Tab firstPinnedTab = mModel.getTabById(mUngroupedTab1.getTabId());
        Tab secondPinnedTab = mModel.getTabById(mGroupedTab1.getTabId());
        firstPinnedTab.setIsPinned(true);
        secondPinnedTab.setIsPinned(true);

        // Select an unpinned tab and a pinned tab.
        selectTabs(mUngroupedTab1, mUngroupedTab2);

        int firstNonPinnedTabIndex = 2;
        when(mModel.findFirstNonPinnedTabIndex()).thenReturn(firstNonPinnedTabIndex);

        // Start reorder on the unpinned tab.
        startReorder(mUngroupedTab2);

        // Verify the pinned tab is moved to the last pinned position and offsetX is toward end.
        verify(mModel).moveTab(eq(mUngroupedTab1.getTabId()), eq(firstNonPinnedTabIndex - 1));
        // 250f(firstUnpinnedTab IdealX) + 0f(firstUnpinnedTab OffsetX) - 0f(lastPinnedTab idealX) -
        // 48f(lastPinnedTab width) = 202f.
        float expectedOffsetX = 202f;
        assertEquals(
                "The pinned tab should have positive offsetX.",
                expectedOffsetX,
                mUngroupedTab1.getOffsetX(),
                DELTA);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testStartReorder_pinnedPrimaryTab_unpinnedTabMovedToFirstNonPinnedPosition() {
        // Pin first two tabs.
        mUngroupedTab1.setIsPinned(true);
        mGroupedTab1.setIsPinned(true);
        Tab firstPinnedTab = mModel.getTabById(mUngroupedTab1.getTabId());
        Tab secondPinnedTab = mModel.getTabById(mGroupedTab1.getTabId());
        firstPinnedTab.setIsPinned(true);
        secondPinnedTab.setIsPinned(true);

        // Select an unpinned tab and a pinned tab.
        selectTabs(mUngroupedTab1, mUngroupedTab2);

        int firstNonPinnedTabIndex = 2;
        when(mModel.findFirstNonPinnedTabIndex()).thenReturn(firstNonPinnedTabIndex);

        // Start reorder on the pinned tab.
        startReorder(mUngroupedTab1);

        // Verify the unpinned tab is moved to the first non-pinned position and offsetX is toward
        // start.
        verify(mModel).moveTab(eq(mUngroupedTab2.getTabId()), eq(firstNonPinnedTabIndex));
        // 0f(lastPinnedTab IdealX) + 0f(lastPinnedTab OffsetX) + 48f(lastPinnedTab width) -
        // 250f(lastUnpinnedTab idealX) = -202f.
        float expectedOffsetX = -202f;
        assertEquals(
                "THe unpinned tab should have negative offsetX.",
                expectedOffsetX,
                mUngroupedTab2.getOffsetX(),
                DELTA);
    }

    // updateReorderPosition success tests
    @Test
    public void testUpdateReorder_success_pastCollapsedGroup() {
        selectTabs(mUngroupedTab2, mUngroupedTab3);
        float rebuildDeltaX = -TAB_WIDTH;
        float dragDeltaX = -DRAG_PAST_COLLAPSED_GROUP_SUCCESS;
        startReorder(mUngroupedTab2);
        testUpdateReorder_success(
                mUngroupedTab2, rebuildDeltaX, dragDeltaX, mCollapsedGroupTitle, 5);
        verifyBlockMovedPastGroup();
    }

    @Test
    public void testUpdateReorder_success_mergeIntoGroup() {
        selectTabs(mUngroupedTab1);
        startReorder(mUngroupedTab1);
        drag(DRAG_INTO_GROUP_SUCCESS);

        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        anyList(), any(Tab.class), eq(0), eq(MergeNotificationType.DONT_NOTIFY));
        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateReorder_success_dragOutOfGroup() {
        // Setup a group with 3 tabs, select 2 of them to drag out.
        reset(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUnGrouper);
        mockTabGroup(
                GROUP_ID1,
                TAB_ID2,
                mModel.getTabById(TAB_ID2),
                mModel.getTabById(TAB_ID3),
                mModel.getTabById(TAB_ID1));
        selectTabs(mGroupedTab1, mGroupedTab2); // Tab2, Tab3
        when(mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(TAB_ID2))).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(TAB_ID3))).thenReturn(true);

        startReorder(mGroupedTab1);
        // Drag right, out of the group.
        drag(DRAG_OUT_OF_GROUP_SUCCESS);

        verify(mTabGroupModelFilter.getTabUngrouper())
                .ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testUpdateReorder_success_dragPinnedTabPastPinnedTab() {
        mUngroupedTab2.setIsPinned(true);
        mUngroupedTab3.setIsPinned(true);
        selectTabs(mUngroupedTab2);
        startReorder(mUngroupedTab2);
        testUpdateReorder_fail(mUngroupedTab2, DRAG_PAST_TAB_SUCCESS);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testUpdateReorder_unpinnedReorderSuccess_dragMixedPinnedUnpinnedPastUnpinnedTab() {
        // Pin first two tabs.
        mUngroupedTab1.setIsPinned(true);
        mGroupedTab1.setIsPinned(true);
        Tab firstPinnedTab = mModel.getTabById(mUngroupedTab1.getTabId());
        Tab secondPinnedTab = mModel.getTabById(mGroupedTab1.getTabId());
        firstPinnedTab.setIsPinned(true);
        secondPinnedTab.setIsPinned(true);

        // Select an unpinned tab and a pinned tab.
        selectTabs(mUngroupedTab1, mUngroupedTab2);

        // Start reorder on the unpinned tab.
        int firstNonPinnedTabIndex = 2;
        when(mModel.findFirstNonPinnedTabIndex()).thenReturn(firstNonPinnedTabIndex);
        startReorder(mUngroupedTab2);

        // Manually reorders the first tab in block to the second position, this should occur in
        // MultiTabReorderStrategy#gatherBlock, which triggers the mModel#moveTab to move the pinned
        // tab to last pinned position and subsequently triggers a #rebuildStripTab.
        StripLayoutTab temp = mStripTabs[0];
        mStripTabs[0] = mStripTabs[1];
        mStripTabs[1] = temp;

        // Verify the unpinned tabs are reordered successful and pinned tab offsetX correctly
        // updated(should anchored to the first unpinned tab).
        float rebuildDeltaX = TAB_WIDTH;
        float dragDeltaX = DRAG_PAST_TAB_SUCCESS;
        testUpdateReorder_success(
                mUngroupedTab2,
                rebuildDeltaX,
                dragDeltaX,
                mUngroupedTab3,
                /* expectedModelIndex= */ 4);

        // 350f(firstUnpinnedTab IdealX) + -35f(firstUnpinnedTab OffsetX) - 100f(lastPinnedTab
        // idealX) - 48f(lastPinnedTab width) = 167f.
        float expectedOffsetX = 167f;
        assertEquals(
                "The offsetX of the pinned tab is incorrect",
                expectedOffsetX,
                mUngroupedTab1.getOffsetX(),
                DELTA);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testUpdateReorder_unpinnedReorderSuccess_dragMixedPinnedUnpinnedPastPinnedTab() {
        // Pin first two tabs.
        mUngroupedTab1.setIsPinned(true);
        mGroupedTab1.setIsPinned(true);
        Tab firstPinnedTab = mModel.getTabById(mUngroupedTab1.getTabId());
        Tab secondPinnedTab = mModel.getTabById(mGroupedTab1.getTabId());
        firstPinnedTab.setIsPinned(true);
        secondPinnedTab.setIsPinned(true);
        when(mTabGroupModelFilter.isTabInTabGroup(secondPinnedTab)).thenReturn(false);

        // Select an unpinned tab and a pinned tab.
        selectTabs(mUngroupedTab1, mUngroupedTab2);

        // Start reorder on the pinned tab.
        int firstNonPinnedTabIndex = 2;
        when(mModel.findFirstNonPinnedTabIndex()).thenReturn(firstNonPinnedTabIndex);
        when(mPinnedTabsBoundarySupplier.get()).thenReturn(100f);
        startReorder(mUngroupedTab1);
        mUngroupedTab2.setIdealX(44f);

        // Verify the pinned tabs are reordered successfully and unpinned tab offsetX correctly
        // updated(should anchored to the last pinned tab).
        float rebuildDeltaX = TAB_WIDTH;
        float dragDeltaX = 45f;
        testUpdateReorder_success(
                mUngroupedTab1,
                rebuildDeltaX,
                dragDeltaX,
                mGroupedTab1,
                /* expectedModelIndex= */ 0);

        // 50f(lastPinnedTab IdealX) + -5f(firstUnpinnedTab OffsetX) + 48f(lastPinnedTab width) -
        // 94f(lastUnpinnedTab idealX) = -1f.
        float expectedOffsetX = -1f;
        assertEquals(
                "The offsetX of the unpinned tab is incorrect",
                expectedOffsetX,
                mUngroupedTab2.getOffsetX(),
                DELTA);
    }

    // updateReorderPosition failure tests
    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testUpdateReorder_fail_dragPinnedTabPastUnpinnedTab() {
        mUngroupedTab2.setIsPinned(true);
        selectTabs(mUngroupedTab2);
        startReorder(mUngroupedTab2);
        testUpdateReorder_fail(mUngroupedTab2, DRAG_PAST_TAB_SUCCESS);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateReorder_fail_pastTab() {
        selectTabs(mUngroupedTab2);
        startReorder(mUngroupedTab2);
        testUpdateReorder_fail(mUngroupedTab2, DRAG_PAST_TAB_FAIL);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateReorder_fail_pastCollapsedGroup() {
        selectTabs(mUngroupedTab2, mUngroupedTab3);
        startReorder(mUngroupedTab2);
        testUpdateReorder_fail(mUngroupedTab2, -DRAG_PAST_COLLAPSED_GROUP_FAIL);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateReorder_fail_mergeIntoGroup() {
        selectTabs(mUngroupedTab1);
        startReorder(mUngroupedTab1);
        testUpdateReorder_fail(mUngroupedTab1, DRAG_INTO_GROUP_FAIL);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateReorder_fail_dragOutOfGroup() {
        selectTabs(mGroupedTab1, mGroupedTab2);
        startReorder(mGroupedTab1);

        when(mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(TAB_ID2))).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(TAB_ID3))).thenReturn(true);
        testUpdateReorder_fail(mGroupedTab1, DRAG_OUT_OF_GROUP_FAIL);
    }

    // stopReorderMode tests
    @Test
    public void testStopReorder() {
        // Start reorder
        selectTabs(mUngroupedTab1, mUngroupedTab2);
        startReorder(mUngroupedTab1);
        assertTrue(mUngroupedTab1.isForegrounded());
        assertNotNull(mStrategy.getInteractingView());

        // Stop reorder
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);

        // Verify state is reset. The cleanup runs in an onAnimationEnd runnable.
        assertNull("Interacting view should be null after stop", mStrategy.getInteractingView());
        // Verify animation is started to settle tabs.
        verify(mAnimationHost, times(2)).startAnimations(anyList(), any());
    }

    // Helper Methods
    private void selectTabs(StripLayoutTab... tabs) {
        mSelectedTabs.clear();
        mSelectedTabsIds.clear();
        mPinnedTabs.clear();
        mUnpinnedTabs.clear();
        mSelectedTabs.addAll(Arrays.asList(tabs));
        for (StripLayoutTab tab : tabs) {
            mSelectedTabsIds.add(tab.getTabId());
            if (tab.getIsPinned()) {
                mPinnedTabs.add(tab);
            } else {
                mUnpinnedTabs.add(tab);
            }
        }
    }

    private void startReorder(StripLayoutTab primaryTab) {
        mInteractingTab = primaryTab;
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
        mInReorderModeSupplier.set(true);
    }

    private void drag(float deltaX) {
        mStrategy.updateReorderPosition(
                mStripViews,
                mGroupTitles,
                mStripTabs,
                mInteractingTab.getDrawX() + deltaX,
                deltaX,
                ReorderType.DRAG_WITHIN_STRIP);
    }

    private void testUpdateReorder_success(
            StripLayoutTab primaryTab,
            float rebuildDeltaX,
            float dragDeltaX,
            StripLayoutView viewToMove,
            int expectedModelIndex) {
        mockRebuildForBlockMove(viewToMove, rebuildDeltaX);
        drag(dragDeltaX);

        verify(mAnimationHost, times(2)).startAnimations(anyList(), isNull());
        float expectedOffset = dragDeltaX - rebuildDeltaX;
        for (StripLayoutTab tab : primaryTab.getIsPinned() ? mPinnedTabs : mUnpinnedTabs) {
            assertEquals(
                    "Unexpected offset for tab " + tab.getTabId(),
                    expectedOffset,
                    tab.getOffsetX(),
                    DELTA);
        }

        if (viewToMove instanceof StripLayoutTab) {
            verify(mModel)
                    .moveTab(eq(((StripLayoutTab) viewToMove).getTabId()), eq(expectedModelIndex));
        } else if (viewToMove instanceof StripLayoutGroupTitle) {
            verify(mTabGroupModelFilter).moveRelatedTabs(anyInt(), eq(expectedModelIndex));
        }
    }

    private void testUpdateReorder_fail(StripLayoutTab primaryTab, float dragDeltaX) {
        drag(dragDeltaX);
        verifyFailedDrag(dragDeltaX);
    }

    // Mock Helpers
    private void mockRebuildForBlockMove(StripLayoutView viewToMove, float deltaX) {
        if (viewToMove instanceof StripLayoutTab) {
            doAnswer(
                            invocation -> {
                                for (StripLayoutTab tab : mSelectedTabs) {
                                    tab.setIdealX(tab.getIdealX() + deltaX);
                                }
                                int id = invocation.getArgument(0);
                                int index =
                                        Math.min(mStripTabs.length - 1, invocation.getArgument(1));
                                mStripTabs[index] = StripLayoutUtils.findTabById(mStripTabs, id);
                                return null;
                            })
                    .when(mModel)
                    .moveTab(eq(((StripLayoutTab) viewToMove).getTabId()), anyInt());
        } else if (viewToMove instanceof StripLayoutGroupTitle) {
            doAnswer(
                            invocation -> {
                                for (StripLayoutTab tab : mSelectedTabs) {
                                    tab.setIdealX(tab.getIdealX() + deltaX);
                                }
                                int id = invocation.getArgument(0);
                                int index =
                                        Math.min(mStripTabs.length - 1, invocation.getArgument(1));
                                mStripTabs[index] = StripLayoutUtils.findTabById(mStripTabs, id);
                                return null;
                            })
                    .when(mTabGroupModelFilter)
                    .moveRelatedTabs(anyInt(), anyInt());
        }
    }

    // Verification Helpers
    private void verifyBlockMovedPastGroup() {
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(anyInt(), anyInt());
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void verifyFailedDrag(float expectedOffset) {
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        verify(mTabGroupModelFilter.getTabUngrouper(), times(1))
                .ungroupTabs(anyList(), anyBoolean(), anyBoolean(), any());

        verify(mAnimationHost, times(1)).startAnimations(anyList(), isNull());

        for (StripLayoutTab tab : mSelectedTabs) {
            assertEquals(
                    "Unexpected offset for tab " + tab.getTabId(),
                    expectedOffset,
                    tab.getOffsetX(),
                    DELTA);
        }
    }
}
