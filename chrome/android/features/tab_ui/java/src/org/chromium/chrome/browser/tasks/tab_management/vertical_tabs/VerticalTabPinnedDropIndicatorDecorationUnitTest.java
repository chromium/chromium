// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;

/** Unit tests for {@link VerticalTabPinnedDropIndicatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final.
        })
public class VerticalTabPinnedDropIndicatorDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mPinnedTabsRecyclerView;
    @Mock private RecyclerView.State mState;

    @Captor private ArgumentCaptor<RectF> mRectCaptor;
    @Captor private ArgumentCaptor<Paint> mPaintCaptor;

    private Activity mActivity;
    private VerticalTabPinnedDropIndicatorDecoration mDecoration;
    private int mThickness;
    private int mItemGap;
    private int mItemHeight;

    @Before
    public void setUp() {
        LocalizationUtils.setRtlForTesting(false);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mThickness =
                mActivity.getResources().getDimensionPixelSize(R.dimen.vertical_tab_spine_width);
        mItemGap =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        mItemHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_height);

        when(mPinnedTabsRecyclerView.getContext()).thenReturn(mActivity);
        when(mPinnedTabsRecyclerView.getWidth()).thenReturn(300);
        when(mPinnedTabsRecyclerView.getHeight()).thenReturn(200);
        when(mPinnedTabsRecyclerView.getPaddingLeft()).thenReturn(12);
        when(mPinnedTabsRecyclerView.getPaddingRight()).thenReturn(12);
        when(mPinnedTabsRecyclerView.getPaddingTop()).thenReturn(8);
        when(mPinnedTabsRecyclerView.getPaddingBottom()).thenReturn(8);

        mDecoration = new VerticalTabPinnedDropIndicatorDecoration(mActivity);
    }

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
    }

    private View createChildView(int left, int top, int right, int bottom) {
        View view = new View(mActivity);
        view.layout(left, top, right, bottom);
        view.setLeft(left);
        view.setTop(top);
        view.setRight(right);
        view.setBottom(bottom);
        view.setLayoutParams(new ViewGroup.LayoutParams(right - left, bottom - top));
        return view;
    }

    @Test
    @SmallTest
    public void testOnDrawOver_MainListTarget_DoesNotDraw() {
        DropTargetResult mainListResult =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 1,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        /* targetViewHolder= */ null,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 100, 288, 132));

        mDecoration.setDropTargetResult(mainListResult);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ZeroPinnedState_DoesNotDraw() {
        DropTargetResult zeroPinnedResult =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 0,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ true,
                        /* isZeroNormalTabsState= */ false,
                        /* targetViewHolder= */ null,
                        /* adapterPosition= */ 0,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(0, 0, 300, 0));

        mDecoration.setDropTargetResult(zeroPinnedResult);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_InsertBefore_LTR_DrawsVerticalBarToLeftOfItem() {
        View childView = createChildView(100, 20, 142, 52);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.PINNED_GRID,
                        /* destTabIndex= */ 1,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(100, 20, 142, 52));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        mPaintCaptor.capture());

        RectF drawnRect = mRectCaptor.getValue();
        float expectedCenterX = 100f - mItemGap / 2.0f;
        float expectedLeft = expectedCenterX - mThickness / 2.0f;
        float expectedRight = expectedCenterX + mThickness / 2.0f;

        assertEquals(expectedLeft, drawnRect.left, 0.01f);
        assertEquals(expectedRight, drawnRect.right, 0.01f);
        assertEquals(20f, drawnRect.top, 0.01f);
        assertEquals(52f, drawnRect.bottom, 0.01f);

        assertEquals(
                SemanticColorUtils.getColorPrimary(mActivity), mPaintCaptor.getValue().getColor());
    }

    @Test
    @SmallTest
    public void testOnDrawOver_InsertAfter_LTR_DrawsVerticalBarToRightOfItem() {
        View childView = createChildView(100, 20, 142, 52);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.PINNED_GRID,
                        /* destTabIndex= */ 2,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ false,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(100, 20, 142, 52));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        float expectedCenterX = 142f + mItemGap / 2.0f;
        float expectedLeft = expectedCenterX - mThickness / 2.0f;
        float expectedRight = expectedCenterX + mThickness / 2.0f;

        assertEquals(expectedLeft, drawnRect.left, 0.01f);
        assertEquals(expectedRight, drawnRect.right, 0.01f);
        assertEquals(20f, drawnRect.top, 0.01f);
        assertEquals(52f, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_InsertBefore_RTL_DrawsVerticalBarToRightOfItem() {
        LocalizationUtils.setRtlForTesting(true);

        View childView = createChildView(100, 20, 142, 52);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.PINNED_GRID,
                        /* destTabIndex= */ 1,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(100, 20, 142, 52));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        float expectedCenterX = 142f + mItemGap / 2.0f;
        float expectedLeft = expectedCenterX - mThickness / 2.0f;
        float expectedRight = expectedCenterX + mThickness / 2.0f;

        assertEquals(expectedLeft, drawnRect.left, 0.01f);
        assertEquals(expectedRight, drawnRect.right, 0.01f);
        assertEquals(20f, drawnRect.top, 0.01f);
        assertEquals(52f, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_InsertAfter_RTL_DrawsVerticalBarToLeftOfItem() {
        LocalizationUtils.setRtlForTesting(true);

        View childView = createChildView(100, 20, 142, 52);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.PINNED_GRID,
                        /* destTabIndex= */ 2,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ false,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(100, 20, 142, 52));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        float expectedCenterX = 100f - mItemGap / 2.0f;
        float expectedLeft = expectedCenterX - mThickness / 2.0f;
        float expectedRight = expectedCenterX + mThickness / 2.0f;

        assertEquals(expectedLeft, drawnRect.left, 0.01f);
        assertEquals(expectedRight, drawnRect.right, 0.01f);
        assertEquals(20f, drawnRect.top, 0.01f);
        assertEquals(52f, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_FallbackToAnchorBounds_WhenViewHolderUnattached() {
        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.PINNED_GRID,
                        /* destTabIndex= */ 0,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ true,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        /* targetViewHolder= */ null,
                        /* adapterPosition= */ 0,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 8, 54, 40));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        float expectedCenterX = 12f - mItemGap / 2.0f;
        // Min clamped to paddingLeft (12) + thickness / 2
        float minCenterX = 12f + mThickness / 2.0f;
        float clampedCenterX = Math.max(expectedCenterX, minCenterX);
        float expectedLeft = clampedCenterX - mThickness / 2.0f;
        float expectedRight = clampedCenterX + mThickness / 2.0f;

        assertEquals(expectedLeft, drawnRect.left, 0.01f);
        assertEquals(expectedRight, drawnRect.right, 0.01f);
        assertEquals(8f, drawnRect.top, 0.01f);
        assertEquals(40f, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ZeroNormalTabsState_DrawsHorizontalBarAtBottomOfPinnedGrid() {
        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 2,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ true,
                        /* targetViewHolder= */ null,
                        /* adapterPosition= */ 0,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(0, 0, 300, 0));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mPinnedTabsRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        mPaintCaptor.capture());

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);

        float expectedCenterY =
                200f - mThickness / VerticalTabPinnedDropIndicatorDecoration.INDICATOR_DIVISOR;
        float expectedTop =
                expectedCenterY
                        - mThickness / VerticalTabPinnedDropIndicatorDecoration.INDICATOR_DIVISOR;
        float expectedBottom =
                expectedCenterY
                        + mThickness / VerticalTabPinnedDropIndicatorDecoration.INDICATOR_DIVISOR;
        assertEquals(expectedTop, drawnRect.top, 0.01f);
        assertEquals(expectedBottom, drawnRect.bottom, 0.01f);

        assertEquals(
                SemanticColorUtils.getColorPrimary(mActivity), mPaintCaptor.getValue().getColor());
    }
}
