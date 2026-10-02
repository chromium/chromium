// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VerticalPinnedTabListRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalPinnedTabListRecyclerViewUnitTest {
    private static final int SPAN_COUNT = 4;

    private Activity mActivity;
    private VerticalPinnedTabListRecyclerView mRecyclerView;
    private RecyclerView.ItemDecoration mDecoration;
    private int mMinPinnedTabGap;
    private int mMinPinnedTabWidth;
    private TabListModel mPinnedTabsModelList;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRecyclerView =
                new VerticalPinnedTabListRecyclerView(
                        mActivity, Robolectric.buildAttributeSet().build());
        mRecyclerView.setLayoutManager(new GridLayoutManager(mActivity, SPAN_COUNT));
        mDecoration = VerticalPinnedTabListRecyclerView.createItemDecoration();
        mRecyclerView.addItemDecoration(mDecoration);

        Resources res = mActivity.getResources();
        mMinPinnedTabGap = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        mMinPinnedTabWidth = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_min_width);
    }

    @Test
    public void testCalculateBalancedSpanCount_NoPinnedTabs_ReturnsMaxFitSpans() {
        assertEquals(2, calculateBalancedSpanCount(getWidthForColumns(2), /* pinnedTabCount= */ 0));
    }

    @Test
    public void testCalculateBalancedSpanCount_BalancesTabsAcrossRows() {
        int width = getWidthForColumns(4);
        // N=1 -> 1, N=2 -> 2, N=3 -> 3, N=4 -> 4, N=5 -> 3 (3 + 2), N=6 -> 3 (3 + 3).
        int[] expectedSpans = {1, 2, 3, 4, 3, 3};
        for (int i = 0; i < expectedSpans.length; i++) {
            assertEquals(
                    "Span count mismatch for " + (i + 1) + " pinned tab(s)",
                    expectedSpans[i],
                    calculateBalancedSpanCount(width, i + 1));
        }
    }

    @Test
    public void testCalculateBalancedSpanCount_CappedAtMaxSingleRowSpanCount() {
        int width = getWidthForColumns(7);
        assertEquals(
                VerticalPinnedTabListRecyclerView.MAX_SINGLE_ROW_SPAN_COUNT,
                calculateBalancedSpanCount(width, /* pinnedTabCount= */ 0));
        for (int pinnedTabCount = 1; pinnedTabCount <= 10; pinnedTabCount++) {
            int spanCount = calculateBalancedSpanCount(width, pinnedTabCount);
            assertTrue(
                    "Span count (" + spanCount + ") for " + pinnedTabCount + " tabs should be <= 5",
                    spanCount <= VerticalPinnedTabListRecyclerView.MAX_SINGLE_ROW_SPAN_COUNT);
        }
    }

    @Test
    public void testCalculateBalancedSpanCount_TooNarrow_ReturnsOne() {
        assertEquals(1, calculateBalancedSpanCount(/* availableWidth= */ 0, 3));
    }

    @Test
    public void testInitialize_SetsAdapterAndGridLayoutManager() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();

        assertEquals(0, recyclerView.getAdapter().getItemCount());
        mPinnedTabsModelList.add(createTabListItem());
        assertEquals(1, recyclerView.getAdapter().getItemCount());

        assertTrue(recyclerView.getLayoutManager() instanceof GridLayoutManager);
        assertEquals(
                VerticalPinnedTabListRecyclerView.DEFAULT_GRID_SPAN_COUNT,
                ((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount());
        assertEquals(1, recyclerView.getItemDecorationCount());
    }

    @Test
    public void testInitialize_MapsTabTypeToPinnedTabType() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();
        mPinnedTabsModelList.add(createTabListItem());

        assertEquals(UiType.PINNED_TAB, recyclerView.getAdapter().getItemViewType(0));
    }

    @Test
    public void testRecycleItemViews_KeepsAdapter() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();
        RecyclerView.Adapter<?> adapter = recyclerView.getAdapter();

        recyclerView.recycleItemViews();

        assertEquals(adapter, recyclerView.getAdapter());
    }

    @Test
    public void testUpdateSpanCount_UpdatesLayoutManager() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();

        recyclerView.updateSpanCount(RailCollapseState.COLLAPSED);

        assertEquals(
                VerticalPinnedTabListRecyclerView.COLLAPSED_GRID_SPAN_COUNT,
                ((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount());
    }

    @Test
    public void testCalculateSpanCount_CollapsedPositioning_ReturnsCollapsedSpanCount() {
        assertEquals(
                VerticalPinnedTabListRecyclerView.COLLAPSED_GRID_SPAN_COUNT,
                mRecyclerView.calculateSpanCount(RailCollapseState.COLLAPSED));
        assertEquals(
                VerticalPinnedTabListRecyclerView.COLLAPSED_GRID_SPAN_COUNT,
                mRecyclerView.calculateSpanCount(RailCollapseState.EXPANDED_FOR_HOVERING));
    }

    @Test
    public void testCalculateSpanCount_NoParent_ReturnsDefaultSpanCount() {
        assertEquals(
                VerticalPinnedTabListRecyclerView.DEFAULT_GRID_SPAN_COUNT,
                mRecyclerView.calculateSpanCount(RailCollapseState.EXPANDED));
    }

    @Test
    public void testCalculateSpanCount_UsesParentWidthExcludingPadding() {
        int padding = 10;
        FrameLayout parent = new FrameLayout(mActivity);
        parent.setPaddingRelative(padding, 0, padding, 0);
        parent.addView(mRecyclerView);
        parent.layout(0, 0, getWidthForColumns(2) + 2 * padding, 100);

        assertEquals(2, mRecyclerView.calculateSpanCount(RailCollapseState.EXPANDED));
    }

    @Test
    public void testSetInTransition_RequestsLayoutOnlyOnChange() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();
        layoutAndClearRequest(recyclerView);

        recyclerView.setInTransition(true);
        assertTrue(recyclerView.isLayoutRequested());

        layoutAndClearRequest(recyclerView);
        recyclerView.setInTransition(true);
        assertFalse(recyclerView.isLayoutRequested());

        recyclerView.setInTransition(false);
        assertTrue(recyclerView.isLayoutRequested());
    }

    @Test
    public void testCalculatePinnedExtraLayoutSpace_NotTransitioning() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();
        int[] extraLayoutSpace = new int[2];
        RecyclerView.State state = mock(RecyclerView.State.class);
        when(state.getItemCount()).thenReturn(30);

        recyclerView.calculatePinnedExtraLayoutSpace(state, extraLayoutSpace);

        assertEquals(0, extraLayoutSpace[0]);
        assertEquals(0, extraLayoutSpace[1]);
    }

    @Test
    public void testCalculatePinnedExtraLayoutSpace_InTransition() {
        VerticalPinnedTabListRecyclerView recyclerView = createInitializedRecyclerView();
        recyclerView.setInTransition(true);
        int[] extraLayoutSpace = new int[2];
        RecyclerView.State state = mock(RecyclerView.State.class);

        int itemHeight =
                TabVerticalViewBinder.getPinnedItemHeight(mActivity)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.vertical_tab_pinned_item_margin_bottom);
        int padding = recyclerView.getPaddingTop() + recyclerView.getPaddingBottom();

        // 0 items: falls back to container/display height.
        when(state.getItemCount()).thenReturn(0);
        recyclerView.calculatePinnedExtraLayoutSpace(state, extraLayoutSpace);
        int baseHeight = extraLayoutSpace[0];
        assertTrue(baseHeight > 0);
        assertEquals(baseHeight, extraLayoutSpace[1]);

        // Many items: scales with total content height.
        extraLayoutSpace[0] = 0;
        extraLayoutSpace[1] = 0;
        when(state.getItemCount()).thenReturn(30);
        recyclerView.calculatePinnedExtraLayoutSpace(state, extraLayoutSpace);
        int expectedHeight = 30 * itemHeight + padding;
        assertEquals(expectedHeight, extraLayoutSpace[0]);
        assertEquals(expectedHeight, extraLayoutSpace[1]);

        // Excessive items: capped at baseHeight * MAX_SINGLE_ROW_SPAN_COUNT + padding.
        extraLayoutSpace[0] = 0;
        extraLayoutSpace[1] = 0;
        when(state.getItemCount()).thenReturn(500);
        recyclerView.calculatePinnedExtraLayoutSpace(state, extraLayoutSpace);
        int expectedCap =
                baseHeight * VerticalPinnedTabListRecyclerView.MAX_SINGLE_ROW_SPAN_COUNT + padding;
        assertEquals(expectedCap, extraLayoutSpace[0]);
        assertEquals(expectedCap, extraLayoutSpace[1]);
    }

    @Test
    public void testItemDecoration_OffsetsAcrossColumnsAndRows() {
        View child0 = addChild();

        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, child0, mRecyclerView, new RecyclerView.State());
        assertEquals(0, outRect.left);
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect.right);
    }

    @Test
    public void testItemDecoration_OffsetsAcrossColumnsAndRows_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        View child0 = addChild();

        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, child0, mRecyclerView, new RecyclerView.State());
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testItemDecoration_OffsetsCorrectAcrossColumnsAndAfterMove() {
        // Add 4 children representing 4 columns (spanCount = 4).
        View child0 = new View(mActivity);
        View child1 = new View(mActivity);
        View child2 = new View(mActivity);
        View child3 = new View(mActivity);

        // Give child1 a stale LayoutParams with spanIndex = 0 (as if it was moved from position 0).
        GridLayoutManager.LayoutParams lp1 = createLayoutParams();
        ReflectionHelpers.setField(lp1, "mSpanIndex", 0);
        child1.setLayoutParams(lp1);

        mRecyclerView.addView(child0);
        mRecyclerView.addView(child1);
        mRecyclerView.addView(child2);
        mRecyclerView.addView(child3);

        Rect outRect0 = new Rect();
        Rect outRect1 = new Rect();
        Rect outRect2 = new Rect();
        Rect outRect3 = new Rect();

        mDecoration.getItemOffsets(outRect0, child0, mRecyclerView, new RecyclerView.State());
        mDecoration.getItemOffsets(outRect1, child1, mRecyclerView, new RecyclerView.State());
        mDecoration.getItemOffsets(outRect2, child2, mRecyclerView, new RecyclerView.State());
        mDecoration.getItemOffsets(outRect3, child3, mRecyclerView, new RecyclerView.State());

        // Column 0: 0px left, 3/4 gap right
        assertEquals(0, outRect0.left);
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect0.right);

        // Column 1 (despite stale spanIndex=0): 1/4 gap left, 2/4 gap right
        assertEquals(mMinPinnedTabGap / 4, outRect1.left);
        assertEquals(mMinPinnedTabGap - 2 * mMinPinnedTabGap / 4, outRect1.right);

        // Inter-item gap between child 0 and child 1 equals mMinPinnedTabGap.
        assertEquals(mMinPinnedTabGap, outRect0.right + outRect1.left);

        // Column 2: 2/4 gap left, 1/4 gap right
        assertEquals(2 * mMinPinnedTabGap / 4, outRect2.left);
        assertEquals(mMinPinnedTabGap - 3 * mMinPinnedTabGap / 4, outRect2.right);
        assertEquals(mMinPinnedTabGap, outRect1.right + outRect2.left);

        // Column 3: 3/4 gap left, 0px right
        assertEquals(3 * mMinPinnedTabGap / 4, outRect3.left);
        assertEquals(0, outRect3.right);
        assertEquals(mMinPinnedTabGap, outRect2.right + outRect3.left);
    }

    @Test
    public void testItemDecoration_SingleColumn_NoOffsets() {
        mRecyclerView.setLayoutManager(
                new GridLayoutManager(
                        mActivity, VerticalPinnedTabListRecyclerView.COLLAPSED_GRID_SPAN_COUNT));
        View child0 = addChild();

        Rect outRect = new Rect(1, 1, 1, 1);
        mDecoration.getItemOffsets(outRect, child0, mRecyclerView, new RecyclerView.State());
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    private VerticalPinnedTabListRecyclerView createInitializedRecyclerView() {
        VerticalPinnedTabListRecyclerView recyclerView =
                new VerticalPinnedTabListRecyclerView(
                        mActivity, Robolectric.buildAttributeSet().build());
        mPinnedTabsModelList = new TabListModel();
        recyclerView.initialize(mPinnedTabsModelList, RailCollapseState.EXPANDED);
        return recyclerView;
    }

    private static ListItem createTabListItem() {
        return new ListItem(
                UiType.TAB, new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build());
    }

    private int calculateBalancedSpanCount(int availableWidth, int pinnedTabCount) {
        return VerticalPinnedTabListRecyclerView.calculateBalancedSpanCount(
                availableWidth, pinnedTabCount, mActivity.getResources(), /* isTablet= */ false);
    }

    private int getWidthForColumns(int columns) {
        return mMinPinnedTabWidth * columns + mMinPinnedTabGap * (columns - 1);
    }

    private static void layoutAndClearRequest(View view) {
        view.measure(
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
        view.layout(0, 0, 200, 200);
    }

    private View addChild() {
        View child = new View(mActivity);
        child.setLayoutParams(createLayoutParams());
        mRecyclerView.addView(child);
        return child;
    }

    private static GridLayoutManager.LayoutParams createLayoutParams() {
        return new GridLayoutManager.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
