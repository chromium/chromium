// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.CirclePagerIndicatorDecoration.getItemPerScreen;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.State;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/** Unit tests for {@link CirclePagerIndicatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CirclePagerIndicatorDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private LinearLayoutManager mLayoutManager;
    @Mock private RecyclerView.State mState;
    @Mock private View mView1;
    @Mock private View mView2;

    private int mIndicatorHeight;

    private float mIndicatorItemDiameter;
    private float mIndicatorRadius;

    private int mIndicatorItemPadding;
    private int mParentViewWidth;
    private int mParentHeight;
    private CirclePagerIndicatorDecoration mDecoration;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mRecyclerView.getAdapter()).thenReturn(mAdapter);
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);

        Resources resources = mContext.getResources();
        mIndicatorItemPadding =
                resources.getDimensionPixelSize(R.dimen.page_indicator_internal_padding);
        mIndicatorItemDiameter = resources.getDimensionPixelSize(R.dimen.page_indicator_dot_size);
        mIndicatorRadius = mIndicatorItemDiameter / 2f;
        mIndicatorHeight =
                (int) mIndicatorItemDiameter
                        + resources.getDimensionPixelSize(R.dimen.page_indicator_top_margin);
        mParentViewWidth = 800;
        mParentHeight = 400;
        when(mRecyclerView.getHeight()).thenReturn(mParentHeight);
        when(mRecyclerView.getMeasuredWidth()).thenReturn(mParentViewWidth);
        when(mRecyclerView.getWidth()).thenReturn(mParentViewWidth);
    }

    @Test
    @SmallTest
    public void testNoScrolling_Phone() {
        testNoScrollingImpl(/* isTablet= */ false);
    }

    @Test
    @SmallTest
    public void testNoScrolling_Tablet() {
        testNoScrollingImpl(/* isTablet= */ true);
    }

    private void testNoScrollingImpl(boolean isTablet) {
        mDecoration = create(isTablet);

        int count = 3;
        when(mAdapter.getItemCount()).thenReturn(count);
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mView1);

        float dotsTotalLength = mIndicatorItemDiameter * count;
        float paddingBetweenItems = (count - 1) * mIndicatorItemPadding;
        float indicatorTotalWidth = dotsTotalLength + paddingBetweenItems;
        float indicatorStartX = (mParentViewWidth - indicatorTotalWidth) / 2f;
        float indicatorStartY = mParentHeight - mIndicatorItemDiameter;
        float itemWidth = mIndicatorItemDiameter + mIndicatorItemPadding;

        // Verifies that when there isn't any scrolling of the recyclerview, it draws inactive dots
        // and a highlighted dot.
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(2))
                .drawCircle(
                        eq(indicatorStartX),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));
        verify(mCanvas)
                .drawCircle(
                        eq(indicatorStartX + itemWidth),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));
        verify(mCanvas)
                .drawCircle(
                        eq(indicatorStartX + itemWidth * 2),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));
    }

    @Test
    @SmallTest
    public void testScrollTheRecyclerView_Phone() {
        mDecoration = create(/* isTablet= */ false);

        int count = 3;
        when(mAdapter.getItemCount()).thenReturn(count);
        float dotsTotalLength = mIndicatorItemDiameter * count;
        float paddingBetweenItems = (count - 1) * mIndicatorItemPadding;
        float indicatorTotalWidth = dotsTotalLength + paddingBetweenItems;
        float indicatorStartX = (mParentViewWidth - indicatorTotalWidth) / 2f;
        float itemWidth = mIndicatorItemDiameter + mIndicatorItemPadding;
        float indicatorStartY = mParentHeight - mIndicatorItemDiameter;

        // Begin to scroll the recyclerview.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mView1);
        when(mView1.getLeft()).thenReturn(10);
        // Verifies that the animation which is the round rectangle is drawn.
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas)
                .drawRoundRect(
                        eq(indicatorStartX - mIndicatorRadius),
                        eq(indicatorStartY - mIndicatorRadius),
                        eq(indicatorStartX + itemWidth + mIndicatorRadius),
                        eq(indicatorStartY + mIndicatorRadius),
                        eq(mIndicatorRadius),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        when(mView1.getLeft()).thenReturn(20);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(2))
                .drawRoundRect(
                        eq(indicatorStartX - mIndicatorRadius),
                        eq(indicatorStartY - mIndicatorRadius),
                        eq(indicatorStartX + itemWidth + mIndicatorRadius),
                        eq(indicatorStartY + mIndicatorRadius),
                        eq(mIndicatorRadius),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        // Scroll to the second item of the recyclerview.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mView2);

        // Verifies that not drawing the animation the second item is shown without scrolling.
        when(mView2.getLeft()).thenReturn(0);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(2))
                .drawRoundRect(
                        eq(indicatorStartX - mIndicatorRadius),
                        eq(indicatorStartY - mIndicatorRadius),
                        eq(indicatorStartX + itemWidth + mIndicatorRadius),
                        eq(indicatorStartY + mIndicatorRadius),
                        eq(mIndicatorRadius),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        // Verifies that drawing the animation again if continue scrolling the recyclerview.
        when(mView2.getLeft()).thenReturn(10);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas)
                .drawRoundRect(
                        eq(indicatorStartX + itemWidth - mIndicatorRadius),
                        eq(indicatorStartY - mIndicatorRadius),
                        eq(indicatorStartX + itemWidth * 2 + mIndicatorRadius),
                        eq(indicatorStartY + mIndicatorRadius),
                        eq(mIndicatorRadius),
                        eq(mIndicatorRadius),
                        any(Paint.class));
    }

    @Test
    @SmallTest
    public void testScrollTheRecyclerView_Tablet() {
        mDecoration = create(/* isTablet= */ true);
        mDecoration.setItemPerScreenForTesting(2);

        int count = 3;
        when(mAdapter.getItemCount()).thenReturn(count);
        float dotsTotalLength = mIndicatorItemDiameter * count;
        float paddingBetweenItems = (count - 1) * mIndicatorItemPadding;
        float indicatorTotalWidth = dotsTotalLength + paddingBetweenItems;
        float indicatorStartX = (mParentViewWidth - indicatorTotalWidth) / 2f;
        float itemWidth = mIndicatorItemDiameter + mIndicatorItemPadding;
        float indicatorStartY = mParentHeight - mIndicatorItemDiameter;

        // Begin to scroll the recyclerview.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mView1);
        when(mLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(1);
        when(mView1.getLeft()).thenReturn(10);

        // Every time when onDrawOver() is called, all of the dots will be first drawn as inactive
        // dots. Use drawAsInactiveDotTimes to log how many times a dot is draw as inactive one (not
        // the highlighted one).
        int drawAsInactiveDotTimes = 1;
        // Verifies that highlighted dot is drawn.
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(drawAsInactiveDotTimes + 1))
                .drawCircle(
                        eq(indicatorStartX + itemWidth),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        drawAsInactiveDotTimes++;
        // Verifies that the same highlighted dot is drawn again.
        when(mView1.getLeft()).thenReturn(20);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(drawAsInactiveDotTimes + 2))
                .drawCircle(
                        eq(indicatorStartX + itemWidth),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        // Scroll to the second item of the recyclerview.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mView2);
        when(mLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(2);

        drawAsInactiveDotTimes++;
        // Verifies that the second doc is highlighted.
        when(mView2.getLeft()).thenReturn(0);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(drawAsInactiveDotTimes + 1))
                .drawCircle(
                        eq(indicatorStartX + itemWidth * 2),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));

        drawAsInactiveDotTimes++;
        // Verifies that the same highlighted dot is drawn again.
        when(mView2.getLeft()).thenReturn(10);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);
        verify(mCanvas, times(drawAsInactiveDotTimes + 2))
                .drawCircle(
                        eq(indicatorStartX + itemWidth * 2),
                        eq(indicatorStartY),
                        eq(mIndicatorRadius),
                        any(Paint.class));
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_Phone() {
        mDecoration = create(/* isTablet= */ false);

        Rect rect = new Rect();
        View view = Mockito.mock(View.class);
        RecyclerView.State state = Mockito.mock(State.class);
        when(mAdapter.getItemCount()).thenReturn(3);
        when(mRecyclerView.getChildAdapterPosition(view)).thenReturn(1);

        mDecoration.getItemOffsetsImpl(rect, view, mRecyclerView, state);
        // Verifies that the page indicator is shown, but no extra padding is added to any view.
        assertEquals(mIndicatorHeight, rect.bottom);
        assertEquals(0, rect.left);
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_DoNotShowPageIndicator() {
        mDecoration = create(/* isTablet= */ true);

        Rect rect = new Rect();
        View view = Mockito.mock(View.class);
        RecyclerView.State state = Mockito.mock(State.class);

        // The recyclerview has only 1 item shown.
        int itemCount = 1;
        when(mAdapter.getItemCount()).thenReturn(itemCount);

        // Sets the tablet as a wide screen.
        int itemPerScreen = 2;
        mDecoration.onDisplayStyleChanged(0, itemPerScreen);
        mDecoration.getItemOffsetsImpl(rect, view, mRecyclerView, state);
        // Verifies that the space of the indicators are removed when the itemCount is less than the
        // itemPerScreen.
        assertEquals(0, rect.bottom);
        assertEquals(0, rect.left);

        itemCount = 2;
        when(mAdapter.getItemCount()).thenReturn(itemCount);
        mDecoration.onDisplayStyleChanged(0, itemPerScreen);
        mDecoration.getItemOffsetsImpl(rect, view, mRecyclerView, state);
        // Verifies that the space of the indicators are removed when the itemCount equals to the
        // itemPerScreen.
        assertEquals(0, rect.bottom);
        assertEquals(0, rect.left);
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_ShowPageIndicator() {
        mDecoration = create(/* isTablet= */ true);

        Rect rect = new Rect();
        View view = Mockito.mock(View.class);
        RecyclerView.State state = Mockito.mock(State.class);
        // The recyclerview has 3 items shown.
        int itemCount = 3;
        when(mAdapter.getItemCount()).thenReturn(itemCount);

        // Sets the tablet as a wide screen.
        int itemPerScreen = 2;
        mDecoration.onDisplayStyleChanged(0, itemPerScreen);
        mDecoration.getItemOffsetsImpl(rect, view, mRecyclerView, state);
        // Verifies that the page indicator is shown when all of the items can't fit in one screen.
        assertEquals(mIndicatorHeight, rect.bottom);
        // Verifies that no extra padding is added for the first child view.
        assertEquals(0, rect.left);

        // Sets the view not be the first child.
        when(mRecyclerView.getChildAdapterPosition(view)).thenReturn(1);
        mDecoration.getItemOffsetsImpl(rect, view, mRecyclerView, state);
        // Verifies that an extra padding is added on the left side of the view.
        assertEquals(mIndicatorHeight, rect.bottom);
        assertEquals(mIndicatorItemPadding, rect.left);
    }

    @Test
    @SmallTest
    public void testGetItemPerScreen() {
        // Sets the tablet as a wide screen.
        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        assertEquals(2, getItemPerScreen(displayStyle));

        // Sets the tablet as a small screen.
        displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        assertEquals(1, getItemPerScreen(displayStyle));
    }

    private CirclePagerIndicatorDecoration create(boolean isTablet) {
        return new CirclePagerIndicatorDecoration(mContext, 0, Color.BLACK, Color.GRAY, isTablet);
    }
}
