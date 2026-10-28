// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.ArrayList;

/** Tests for FeedActionDelegateImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedItemDecorationTest {
    private static final int GUTTER_PADDING = 20;
    private static final int RECYCLER_VIEW_WIDTH = 500;
    private static final int RECYCLER_VIEW_HEIGHT = 1000;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Canvas mCanvas;
    @Mock private RecyclerView.State mState;
    @Mock private FeedSurfaceCoordinator mCoordinator;
    @Mock private HybridListRenderer mRenderer;
    @Mock private ListLayoutHelper mLayoutHelper;
    @Mock private FeedListContentManager mContentManager;
    @Mock private FeedItemDecoration.DrawableProvider mDrawableProvider;
    @Mock private Drawable mTopRoundedDrawable;
    @Mock private Drawable mTopLeftRoundedDrawable;
    @Mock private Drawable mTopRightRoundedDrawable;
    @Mock private Drawable mBottomRoundedDrawable;
    @Mock private Drawable mBottomLeftRoundedDrawable;
    @Mock private Drawable mBottomRightRoundedDrawable;
    @Mock private Drawable mNotRoundedDrawable;
    @Mock private Drawable mAllRoundedDrawable;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private View mView0;
    private View mView1;
    private View mView2;
    private View mView3;
    private View mView4;
    private View mView5;
    private View mView6;
    private final ArrayList<View> mViewList = new ArrayList<>();
    private final ArrayList<Rect> mBoundsList = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mView0 = new View(mActivity);
        mView1 = new View(mActivity);
        mView2 = new View(mActivity);
        mView3 = new View(mActivity);
        mView4 = new View(mActivity);
        mView5 = new View(mActivity);
        mView6 = new View(mActivity);

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

        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setLayoutManager(new FixedBoundsLayoutManager());
        mRecyclerView.setAdapter(
                new RecyclerView.Adapter<RecyclerView.ViewHolder>() {
                    @Override
                    public int getItemCount() {
                        return mViewList.size();
                    }

                    @Override
                    public int getItemViewType(int position) {
                        // Use the position as the view type so each position gets its own view.
                        return position;
                    }

                    @Override
                    public RecyclerView.ViewHolder onCreateViewHolder(
                            ViewGroup parent, int viewType) {
                        return new RecyclerView.ViewHolder(mViewList.get(viewType)) {};
                    }

                    @Override
                    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}
                });

        when(mCoordinator.getContentManager()).thenReturn(mContentManager);
        when(mCoordinator.getHybridListRenderer()).thenReturn(mRenderer);
        when(mCoordinator.getHeaderPosition()).thenReturn(1);
        when(mCoordinator.isHeaderVisible()).thenReturn(true);

        when(mContentManager.getItemCount()).thenReturn(mViewList.size());

        when(mRenderer.getListLayoutHelper()).thenReturn(mLayoutHelper);

        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_top_rounded))
                .thenReturn(mTopRoundedDrawable);
        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_topleft_rounded))
                .thenReturn(mTopLeftRoundedDrawable);
        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_topright_rounded))
                .thenReturn(mTopRightRoundedDrawable);
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
        when(mDrawableProvider.getDrawable(R.drawable.home_surface_ui_background_rounded))
                .thenReturn(mAllRoundedDrawable);
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
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
    public void testDrawForStandardLayout_singleCardInContainment() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * NTP header view
        // *****************
        // *     view2     * NTP header view
        // *****************
        // *     view3     * NTP header view
        // *****************
        // *     view4     * NTP header view
        // *****************
        // *     view5     * <- all rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        int top = 0;
        for (int i = 0; i < mViewList.size(); ++i) {
            mBoundsList.get(i).set(0, top, 500, top + 100);
            top += 100;
        }

        int kHeaderSize = 5;
        when(mCoordinator.getHeaderPosition()).thenReturn(kHeaderSize);

        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        for (int i = 0; i < mViewList.size(); ++i) {
            if (i >= kHeaderSize && i < mViewList.size() - 1) continue;
            verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(i)));
            verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(i)));
            verify(mBottomRoundedDrawable, never()).setBounds(eq(mBoundsList.get(i)));
            verify(mAllRoundedDrawable, never()).setBounds(eq(mBoundsList.get(i)));
        }

        Rect bounds5 = new Rect(mBoundsList.get(5));
        bounds5.bottom += feedItemDecoration.getAdditionalBottomCardPaddingForTesting();
        verify(mAllRoundedDrawable).setBounds(eq(bounds5));
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
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
    public void testDrawForMultiColumnStaggeredLayout_headerInvisible() {
        // *****************
        // *     view0     * NTP header view
        // *     view1     *
        // *****************
        // * view2 * view3 * <- view2: topleft rounded
        // *       *       * <- view3: topright rounded
        // *****************
        // * view4 * view5 * <- view4: bottomleft rounded
        // *       *       * <- view5: bottomright rounded
        // *****************
        // *     view6     * special bottom view
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(true);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mTopLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mTopRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(0)));

        verify(mTopRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));
        verify(mNotRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));
        verify(mTopLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));
        verify(mTopRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));
        verify(mBottomLeftRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));
        verify(mBottomRightRoundedDrawable, never()).setBounds(eq(mBoundsList.get(1)));

        Rect bounds2 = new Rect(mBoundsList.get(2));
        bounds2.right += GUTTER_PADDING * 2;
        verify(mTopLeftRoundedDrawable, times(1)).setBounds(eq(bounds2));

        Rect bounds3 = new Rect(mBoundsList.get(3));
        verify(mTopRightRoundedDrawable, times(1)).setBounds(eq(bounds3));

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
    public void testClipFirstCardTopCorners_headerInvisible() {
        // *****************
        // *     view0     * NTP header view
        // *****************
        // *     view1     * Feed header, hidden
        // *****************
        // *     view2     * <- first card: clipped to the rounded top
        // *****************
        // *     view3     * <- not clipped
        // *****************
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();

        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertTrue(mView2.getClipToOutline());
        // Other cards in the containment aren't clipped.
        assertFalse(mView3.getClipToOutline());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView3.getOutlineProvider());
        // Views outside the containment are left alone.
        assertEquals(ViewOutlineProvider.BACKGROUND, mView0.getOutlineProvider());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView1.getOutlineProvider());
        // The outline uses the containment's corner radius and extends past the 100px card by
        // that radius, so only the top corners are rounded.
        int radius =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.home_surface_ui_background_radius);
        Outline outline = new Outline();
        mView2.getOutlineProvider().getOutline(mView2, outline);
        Rect outlineRect = new Rect();
        assertTrue(outline.getRect(outlineRect));
        assertEquals(new Rect(0, 0, RECYCLER_VIEW_WIDTH, 100 + radius), outlineRect);
        assertEquals(radius, outline.getRadius(), 0.01f);
    }

    @Test
    public void testNoClip_featureDisabled() {
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ false);
        layoutRecyclerView();

        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertFalse(mView2.getClipToOutline());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView2.getOutlineProvider());
    }

    @Test
    public void testNoClip_staggeredLayout() {
        // Same layout as testDrawForMultiColumnStaggeredLayout_headerInvisible.
        when(mCoordinator.useStaggeredLayout()).thenReturn(true);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
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
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();

        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // The clip is only applied in the single-column layout.
        for (View view : mViewList) {
            assertFalse(view.getClipToOutline());
        }
    }

    @Test
    public void testClipOnlyWrittenWhenStateChanges() {
        OutlineWriteCountingView firstCard = new OutlineWriteCountingView(mActivity);
        mViewList.set(2, firstCard);
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();

        // onDraw() runs on every frame while scrolling, so the view must only be written once.
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertTrue(firstCard.getClipToOutline());
        assertEquals(1, firstCard.mOutlineProviderWrites);
        assertEquals(1, firstCard.mClipToOutlineWrites);
    }

    @Test
    public void testClipRemoved_headerBecomesVisible() {
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertTrue(mView2.getClipToOutline());

        // The feed header shows again, so it's at the top of the containment instead of the card.
        when(mCoordinator.isHeaderVisible()).thenReturn(true);
        when(mCoordinator.getHeaderPosition()).thenReturn(1);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertFalse(mView1.getClipToOutline());
        assertFalse(mView2.getClipToOutline());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView2.getOutlineProvider());
    }

    @Test
    public void testClipMovesToNextCard_firstCardDismissed() {
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Dismiss the first card. Until the next layout, its view has no adapter position and the
        // card below it takes over position 2.
        mViewList.remove(mView2);
        when(mContentManager.getItemCount()).thenReturn(mViewList.size());
        mRecyclerView.getAdapter().notifyItemRemoved(2);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // The dismissed card keeps its clip while it animates out.
        assertTrue(mView2.getClipToOutline());
        assertTrue(mView3.getClipToOutline());
        assertFalse(mView4.getClipToOutline());
    }

    @Test
    public void testClipMovesBack_firstCardRestoredByUndo() {
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertTrue(mView2.getClipToOutline());

        // Undo restores a card above the first card, which moves down to position 3.
        View restoredCard = new View(mActivity);
        mViewList.add(2, restoredCard);
        mBoundsList.add(new Rect());
        setStackedBounds();
        when(mContentManager.getItemCount()).thenReturn(mViewList.size());
        mRecyclerView.getAdapter().notifyItemInserted(2);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertTrue(restoredCard.getClipToOutline());
        assertFalse(mView2.getClipToOutline());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView2.getOutlineProvider());
    }

    @Test
    public void testClipRemoved_dismissedCardReusedBelowFirstCard() {
        when(mCoordinator.useStaggeredLayout()).thenReturn(false);
        when(mCoordinator.isHeaderVisible()).thenReturn(false);
        when(mCoordinator.getHeaderPosition()).thenReturn(2);
        setStackedBounds();
        FeedItemDecoration feedItemDecoration =
                createFeedItemDecoration(/* clipFirstCardTopCorners= */ true);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertTrue(mView2.getClipToOutline());

        // Dismiss the first card and let its remove animation finish.
        mViewList.remove(mView2);
        when(mContentManager.getItemCount()).thenReturn(mViewList.size());
        mRecyclerView.getAdapter().notifyItemRemoved(2);
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        layoutRecyclerView();
        mRecyclerView.getItemAnimator().endAnimations();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertTrue(mView3.getClipToOutline());

        // The dismissed card's view is reused further down the feed.
        mViewList.add(4, mView2);
        when(mContentManager.getItemCount()).thenReturn(mViewList.size());
        mRecyclerView.getAdapter().notifyItemInserted(4);
        layoutRecyclerView();
        feedItemDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertTrue(mView3.getClipToOutline());
        assertFalse(mView2.getClipToOutline());
        assertEquals(ViewOutlineProvider.BACKGROUND, mView2.getOutlineProvider());
    }

    /** Lays out the RecyclerView so that each child is placed at its bounds in mBoundsList. */
    private void layoutRecyclerView() {
        mRecyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(RECYCLER_VIEW_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(RECYCLER_VIEW_HEIGHT, View.MeasureSpec.EXACTLY));
        mRecyclerView.layout(0, 0, RECYCLER_VIEW_WIDTH, RECYCLER_VIEW_HEIGHT);
    }

    private FeedItemDecoration createFeedItemDecoration(boolean clipFirstCardTopCorners) {
        return new FeedItemDecoration(
                mActivity,
                mCoordinator,
                mDrawableProvider,
                GUTTER_PADDING,
                clipFirstCardTopCorners);
    }

    /** Stacks the views vertically, each 100px tall and as wide as the RecyclerView. */
    private void setStackedBounds() {
        int top = 0;
        for (Rect bounds : mBoundsList) {
            bounds.set(0, top, RECYCLER_VIEW_WIDTH, top + 100);
            top += 100;
        }
    }

    /** A LayoutManager that places each child at the bounds given by mBoundsList. */
    private class FixedBoundsLayoutManager extends RecyclerView.LayoutManager {
        @Override
        public RecyclerView.LayoutParams generateDefaultLayoutParams() {
            return new RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        @Override
        public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
            detachAndScrapAttachedViews(recycler);
            for (int i = 0; i < state.getItemCount(); ++i) {
                View view = recycler.getViewForPosition(i);
                addView(view);
                Rect bounds = mBoundsList.get(i);
                layoutDecoratedWithMargins(
                        view, bounds.left, bounds.top, bounds.right, bounds.bottom);
            }
        }
    }

    /** A View that counts how often its outline properties are written. */
    private static class OutlineWriteCountingView extends View {
        private int mOutlineProviderWrites;
        private int mClipToOutlineWrites;

        OutlineWriteCountingView(Context context) {
            super(context);
        }

        @Override
        public void setOutlineProvider(ViewOutlineProvider provider) {
            ++mOutlineProviderWrites;
            super.setOutlineProvider(provider);
        }

        @Override
        public void setClipToOutline(boolean clipToOutline) {
            ++mClipToOutlineWrites;
            super.setClipToOutline(clipToOutline);
        }
    }
}
