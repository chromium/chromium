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

/** Unit tests for {@link VerticalTabDropIndicatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final.
        })
public class VerticalTabDropIndicatorDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.State mState;

    @Captor private ArgumentCaptor<RectF> mRectCaptor;
    @Captor private ArgumentCaptor<Paint> mPaintCaptor;

    private Activity mActivity;
    private VerticalTabDropIndicatorDecoration mDecoration;
    private int mThickness;
    private int mMarginBottom;
    private int mNestingMargin;

    @Before
    public void setUp() {
        LocalizationUtils.setRtlForTesting(false);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mThickness =
                mActivity.getResources().getDimensionPixelSize(R.dimen.vertical_tab_spine_width);
        mMarginBottom =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_margin_bottom);
        mNestingMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);

        when(mRecyclerView.getContext()).thenReturn(mActivity);
        when(mRecyclerView.getWidth()).thenReturn(300);
        when(mRecyclerView.getHeight()).thenReturn(1000);
        when(mRecyclerView.getPaddingLeft()).thenReturn(12);
        when(mRecyclerView.getPaddingRight()).thenReturn(12);
        when(mRecyclerView.getPaddingTop()).thenReturn(8);
        when(mRecyclerView.getPaddingBottom()).thenReturn(8);

        mDecoration = new VerticalTabDropIndicatorDecoration(mActivity);
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
    public void testOnDrawOver_PinnedGridTarget_DoesNotDraw() {
        DropTargetResult pinnedResult =
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
                        new Rect(0, 0, 100, 100));

        mDecoration.setDropTargetResult(pinnedResult);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_SingleTab_LTR_InsertBefore() {
        View childView = createChildView(12, 100, 288, 132);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 1,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 100, 288, 132));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        mPaintCaptor.capture());

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);

        float expectedCenterY = 100f - mMarginBottom / 2.0f;
        float expectedTop = expectedCenterY - mThickness / 2.0f;
        float expectedBottom = expectedCenterY + mThickness / 2.0f;
        assertEquals(expectedTop, drawnRect.top, 0.01f);
        assertEquals(expectedBottom, drawnRect.bottom, 0.01f);

        assertEquals(
                SemanticColorUtils.getColorPrimary(mActivity), mPaintCaptor.getValue().getColor());
    }

    @Test
    @SmallTest
    public void testOnDrawOver_SingleTab_LTR_InsertAfter() {
        View childView = createChildView(12, 100, 288, 132);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 2,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 1,
                        /* insertBefore= */ false,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 100, 288, 132));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);

        float expectedCenterY = 132f + mMarginBottom / 2.0f;
        float expectedTop = expectedCenterY - mThickness / 2.0f;
        float expectedBottom = expectedCenterY + mThickness / 2.0f;
        assertEquals(expectedTop, drawnRect.top, 0.01f);
        assertEquals(expectedBottom, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ChildTabInGroup_LTR_IndentsStart() {
        View childView = createChildView(12 + mNestingMargin, 150, 288, 182);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                        /* destGroupTabId= */ 100,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 2,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12 + mNestingMargin, 150, 288, 182));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f + mNestingMargin, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ChildTabInGroup_RTL_IndentsEnd() {
        LocalizationUtils.setRtlForTesting(true);

        View childView = createChildView(12, 150, 288 - mNestingMargin, 182);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(childView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                        /* destGroupTabId= */ 100,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 2,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 150, 288 - mNestingMargin, 182));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f - mNestingMargin, drawnRect.right, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_GroupBoundary_LTR_DrawsFullWidth() {
        View headerView = createChildView(12, 100, 288, 132);
        RecyclerView.ViewHolder vh = new RecyclerView.ViewHolder(headerView) {};

        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 0,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        vh,
                        /* adapterPosition= */ 0,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ true,
                        new Rect(12, 100, 288, 132));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ZeroPinnedState_DrawsAtTopOfRecyclerView() {
        DropTargetResult result =
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

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);

        float expectedCenterY = 8f + mThickness / 2.0f;
        float expectedTop = expectedCenterY - mThickness / 2.0f;
        float expectedBottom = expectedCenterY + mThickness / 2.0f;
        assertEquals(expectedTop, drawnRect.top, 0.01f);
        assertEquals(expectedBottom, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_FallbackToAnchorBounds_WhenViewHolderUnattached() {
        DropTargetResult result =
                new DropTargetResult(
                        DropTargetResult.TargetType.MAIN_LIST,
                        /* destTabIndex= */ 3,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* isPinned= */ false,
                        /* isZeroPinnedState= */ false,
                        /* isZeroNormalTabsState= */ false,
                        /* targetViewHolder= */ null,
                        /* adapterPosition= */ 3,
                        /* insertBefore= */ true,
                        /* isGroupTopOrBottomBoundary= */ false,
                        new Rect(12, 300, 288, 332));

        mDecoration.setDropTargetResult(result);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        any(Paint.class));

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(12f, drawnRect.left, 0.01f);
        assertEquals(288f, drawnRect.right, 0.01f);

        float expectedCenterY = 300f - mMarginBottom / 2.0f;
        float expectedTop = expectedCenterY - mThickness / 2.0f;
        float expectedBottom = expectedCenterY + mThickness / 2.0f;
        assertEquals(expectedTop, drawnRect.top, 0.01f);
        assertEquals(expectedBottom, drawnRect.bottom, 0.01f);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ZeroNormalTabsState_DoesNotDraw() {
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
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }
}
