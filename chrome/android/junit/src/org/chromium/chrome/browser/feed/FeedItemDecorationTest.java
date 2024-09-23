// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.ArrayList;

/** Tests for FeedActionDelegateImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedItemDecorationTest {
    private static final int GUTTER_PADDING = 20;
    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.State mState;
    @Mock private View mView0;
    @Mock private View mView1;
    @Mock private View mView2;
    @Mock private View mView3;
    @Mock private View mView4;
    @Mock private View mView5;
    @Mock private View mView6;
    @Mock private FeedSurfaceCoordinator mCoordinator;
    @Mock private HybridListRenderer mRenderer;
    @Mock private ListLayoutHelper mLayoutHelper;
    @Mock private FeedListContentManager mContentManager;
    @Mock private FeedItemDecoration.DrawableProvider mDrawableProvider;
    @Mock private Drawable mTopRoundedDrawable;
    @Mock private Drawable mBottomRoundedDrawable;
    @Mock private Drawable mBottomLeftRoundedDrawable;
    @Mock private Drawable mBottomRightRoundedDrawable;
    @Mock private Drawable mNotRoundedDrawable;
    private Activity mActivity;
    private ArrayList<View> mViewList = new ArrayList<>();
    private ArrayList<Rect> mBoundsList = new ArrayList<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

        mViewList.add(mView0);
        mViewList.add(mView1);
        mViewList.add(mView2);
        mViewList.add(mView3);
        mViewList.add(mView4);
        mViewList.add(mView5);
        mViewList.add(mView6);

        for (int i = 0; i < mViewList.size(); ++i) {
            mBoundsList.add(new Rect());
        }

        when(mRecyclerView.getChildCount()).thenReturn(mViewList.size());
        for (int i = 0; i < mViewList.size(); ++i) {
            when(mRecyclerView.getChildAt(i)).thenReturn(mViewList.get(i));
            when(mRecyclerView.getChildAdapterPosition(mViewList.get(i))).thenReturn(i);
        }
        doAnswer(
                        invocation -> {
                            View view = invocation.getArgument(0);
                            Rect outBounds = invocation.getArgument(1);

                            for (int i = 0; i < mViewList.size(); ++i) {
                                if (view == mViewList.get(i)) {
                                    outBounds.set(mBoundsList.get(i));
                                }
                            }

                            return null;
                        })
                .when(mRecyclerView)
                .getDecoratedBoundsWithMargins(any(View.class), any(Rect.class));

        when(mCoordinator.getContentManager()).thenReturn(mContentManager);
        when(mCoordinator.getHybridListRenderer()).thenReturn(mRenderer);
        when(mCoordinator.getSectionHeaderPosition()).thenReturn(1);

        when(mContentManager.getItemCount()).thenReturn(mViewList.size());

        when(mRenderer.getListLayoutHelper()).thenReturn(mLayoutHelper);

        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_top_rounded))
                .thenReturn(mTopRoundedDrawable);
        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_bottom_rounded))
                .thenReturn(mBottomRoundedDrawable);
        when(mDrawableProvider.getDrawable(
                        R.drawable.home_surface_ui_background_bottomleft_rounded))
                .thenReturn(mBottomLeftRoundedDrawable);
        when(mDrawableProvider.getDrawable(
                        R.drawable.home_surface_ui_background_bottomright_rounded))
                .thenReturn(mBottomRightRoundedDrawable);
        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_not_rounded))
                .thenReturn(mNotRoundedDrawable);
    }

    @Test
    public void testDrawForStandardLayout() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * <- top rounded
        // *****************
        // *     view2     * <- not rounded
        // *****************
        // *     view3     * <- not rounded
        // *****************
        // *     view4     * <- not rounded
        // *****************
        // *     view5     * <- bottom rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        int top = 0;
        for (int i = 0; i < mViewList.size(); ++i) {
            mBoundsList.get(i).set(0, top, 500, top + 100);
            top += 100;
        }

        FeedItemDecoration feedItemDecoration =
                new FeedItemDecoration(mActivity, mCoordinator, mDrawableProvider, GUTTER_PADDING);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mTopRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(1)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(2)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(3)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(4)));
        Rect bounds5 = new Rect(mBoundsList.get(5));
        bounds5.bottom += feedItemDecoration.getAdditionalBottomCardPaddingForTesting();
        verify(mBottomRoundedDrawable, times(1)).setBounds(eq(bounds5));
        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
    }

    @Test
    public void testDrawForMultiColumnStaggeredLayout() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * <- top rounded
        // *****************
        // * view2 * view3 * <- not rounded
        // *****************
        // * view4 * view5 * <- view4: bottomleft rounded
        // *       * view5 * <- view5: bottomright rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(true);
        when(mLayoutHelper.getColumnIndex(mView0)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView1)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView2)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView3)).thenReturn(1);
        when(mLayoutHelper.getColumnIndex(mView4)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView5)).thenReturn(1);
        when(mLayoutHelper.getColumnIndex(mView6)).thenReturn(-1);
        mBoundsList.get(0).set(0, 0, 500, 100);
        mBoundsList.get(1).set(0, 100, 500, 200);
        mBoundsList.get(2).set(0, 200, 250, 280);
        mBoundsList.get(3).set(250, 200, 500, 300);
        mBoundsList.get(4).set(0, 280, 250, 400);
        mBoundsList.get(5).set(250, 300, 500, 500);
        mBoundsList.get(6).set(0, 500, 500, 600);

        FeedItemDecoration feedItemDecoration =
                new FeedItemDecoration(mActivity, mCoordinator, mDrawableProvider, GUTTER_PADDING);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));

        Rect bounds1 = new Rect(mBoundsList.get(1));
        verify(mTopRoundedDrawable, times(1)).setBounds(eq(bounds1));

        Rect bounds2 = new Rect(mBoundsList.get(2));
        bounds2.right += GUTTER_PADDING * 2;
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(bounds2));

        Rect bounds3 = new Rect(mBoundsList.get(3));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(bounds3));

        Rect bounds5 = new Rect(mBoundsList.get(5));
        bounds5.bottom += feedItemDecoration.getAdditionalBottomCardPaddingForTesting();
        verify(mBottomRightRoundedDrawable, times(1)).setBounds(eq(bounds5));

        Rect bounds4 = new Rect(mBoundsList.get(4));
        bounds4.right += GUTTER_PADDING * 2;
        bounds4.bottom = bounds5.bottom;
        verify(mBottomLeftRoundedDrawable, times(1)).setBounds(eq(bounds4));

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
    }

    @Test
    public void testDrawForMultiColumnStaggeredLayout_fullSpanViewBeyondMultiColumn() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * <- top rounded
        // *****************
        // * view2 * view3 * <- view2: not rounded
        // *       * view3 * <- view3: not rounded
        // *****************
        // *     view4     * <- not rounded
        // *****************
        // *     view5     * <- bottom rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(true);
        when(mLayoutHelper.getColumnIndex(mView0)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView1)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView2)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView3)).thenReturn(1);
        when(mLayoutHelper.getColumnIndex(mView4)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView5)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView6)).thenReturn(-1);
        mBoundsList.get(0).set(0, 0, 500, 100);
        mBoundsList.get(1).set(0, 100, 500, 200);
        mBoundsList.get(2).set(0, 200, 250, 300);
        mBoundsList.get(3).set(250, 200, 500, 250);
        mBoundsList.get(4).set(0, 300, 500, 400);
        mBoundsList.get(5).set(0, 400, 500, 500);
        mBoundsList.get(6).set(0, 500, 500, 600);

        FeedItemDecoration feedItemDecoration =
                new FeedItemDecoration(mActivity, mCoordinator, mDrawableProvider, GUTTER_PADDING);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));

        Rect bounds1 = new Rect(mBoundsList.get(1));
        verify(mTopRoundedDrawable, times(1)).setBounds(eq(bounds1));

        Rect bounds2 = new Rect(mBoundsList.get(2));
        bounds2.right += GUTTER_PADDING * 2;
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(bounds2));

        Rect bounds3 = new Rect(mBoundsList.get(3));
        bounds3.bottom = bounds2.bottom;
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(bounds3));

        Rect bounds4 = new Rect(mBoundsList.get(4));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(bounds4));

        Rect bounds5 = new Rect(mBoundsList.get(5));
        bounds5.bottom += feedItemDecoration.getAdditionalBottomCardPaddingForTesting();
        verify(mBottomRoundedDrawable, times(1)).setBounds(eq(bounds5));

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
    }

    @Test
    public void testDrawForSingleColumnStaggeredLayout() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * <- top rounded
        // *****************
        // *     view2     * <- not rounded
        // *****************
        // *     view3     * <- not rounded
        // *****************
        // *     view4     * <- not rounded
        // *****************
        // *     view5     * <- bottom rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(true);
        when(mLayoutHelper.getColumnIndex(mView0)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView1)).thenReturn(-1);
        when(mLayoutHelper.getColumnIndex(mView2)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView3)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView4)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView5)).thenReturn(0);
        when(mLayoutHelper.getColumnIndex(mView6)).thenReturn(-1);
        mBoundsList.get(0).set(0, 0, 500, 100);
        mBoundsList.get(1).set(0, 100, 500, 200);
        mBoundsList.get(2).set(0, 200, 500, 300);
        mBoundsList.get(3).set(0, 300, 500, 400);
        mBoundsList.get(4).set(0, 400, 500, 500);
        mBoundsList.get(5).set(0, 500, 500, 600);
        mBoundsList.get(6).set(0, 600, 500, 700);

        FeedItemDecoration feedItemDecoration =
                new FeedItemDecoration(mActivity, mCoordinator, mDrawableProvider, GUTTER_PADDING);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mTopRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(1)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(2)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(3)));
        verify(mNotRoundedDrawable, times(1)).setBounds(eq(mBoundsList.get(4)));
        Rect bounds5 = new Rect(mBoundsList.get(5));
        bounds5.bottom += feedItemDecoration.getAdditionalBottomCardPaddingForTesting();
        verify(mBottomRoundedDrawable, times(1)).setBounds(eq(bounds5));
        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
        verify(mBottomRoundedDrawable, never()).setBounds(eq(mBoundsList.get(6)));
    }
}
