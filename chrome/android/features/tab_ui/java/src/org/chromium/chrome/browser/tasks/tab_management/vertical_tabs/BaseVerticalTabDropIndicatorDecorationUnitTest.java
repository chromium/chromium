// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.filters.SmallTest;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Unit tests for {@link BaseVerticalTabDropIndicatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final.
        })
public class BaseVerticalTabDropIndicatorDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.State mState;

    @Captor private ArgumentCaptor<RectF> mRectCaptor;
    @Captor private ArgumentCaptor<Paint> mPaintCaptor;

    private Activity mActivity;
    private TestDropIndicatorDecoration mDecoration;
    private int mThickness;

    private static class TestDropIndicatorDecoration
            extends BaseVerticalTabDropIndicatorDecoration {
        private boolean mShouldDraw = true;
        private boolean mCalculateBoundsResult = true;
        private @Nullable RectF mBoundsToSet;

        TestDropIndicatorDecoration(Context context) {
            super(context);
        }

        void setShouldDraw(boolean shouldDraw) {
            mShouldDraw = shouldDraw;
        }

        void setCalculateBoundsResult(boolean calculateBoundsResult) {
            mCalculateBoundsResult = calculateBoundsResult;
        }

        void setBoundsToSet(@Nullable RectF bounds) {
            mBoundsToSet = bounds;
        }

        @Override
        protected boolean shouldDraw(DropTargetResult result) {
            return mShouldDraw;
        }

        @Override
        protected boolean calculateBounds(
                RectF outRect, RecyclerView parent, DropTargetResult result) {
            if (mBoundsToSet != null) {
                outRect.set(mBoundsToSet);
            }
            return mCalculateBoundsResult;
        }

        @Nullable View callGetAttachedTargetView(DropTargetResult result, RecyclerView parent) {
            return getAttachedTargetView(result, parent);
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mThickness =
                mActivity.getResources().getDimensionPixelSize(R.dimen.vertical_tab_spine_width);

        when(mRecyclerView.getContext()).thenReturn(mActivity);

        mDecoration = new TestDropIndicatorDecoration(mActivity);
    }

    private DropTargetResult createTestDropTargetResult(@Nullable ViewHolder vh) {
        return new DropTargetResult(
                DropTargetResult.TargetType.MAIN_LIST,
                /* destTabIndex= */ 0,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* isPinned= */ false,
                /* isZeroPinnedState= */ false,
                /* isZeroNormalTabsState= */ false,
                vh,
                /* adapterPosition= */ 0,
                /* insertBefore= */ true,
                /* isGroupTopOrBottomBoundary= */ false,
                new Rect(0, 0, 100, 50));
    }

    @Test
    @SmallTest
    public void testLifecycle_SetGetClear() {
        assertNull(mDecoration.getDropTargetResult());

        DropTargetResult result = createTestDropTargetResult(null);
        mDecoration.setDropTargetResult(result);
        assertSame(result, mDecoration.getDropTargetResult());

        mDecoration.clear();
        assertNull(mDecoration.getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testOnDrawOver_NullTarget_NoOp() {
        mDecoration.setDropTargetResult(null);
        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_ShouldDrawFalse_NoOp() {
        DropTargetResult result = createTestDropTargetResult(null);
        mDecoration.setDropTargetResult(result);
        mDecoration.setShouldDraw(false);

        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_CalculateBoundsFalse_NoOp() {
        DropTargetResult result = createTestDropTargetResult(null);
        mDecoration.setDropTargetResult(result);
        mDecoration.setShouldDraw(true);
        mDecoration.setCalculateBoundsResult(false);

        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verifyNoInteractions(mCanvas);
    }

    @Test
    @SmallTest
    public void testOnDrawOver_RendersRoundRectWithPrimaryColor() {
        DropTargetResult result = createTestDropTargetResult(null);
        mDecoration.setDropTargetResult(result);
        mDecoration.setShouldDraw(true);
        mDecoration.setCalculateBoundsResult(true);

        RectF expectedBounds = new RectF(10f, 20f, 200f, 24f);
        mDecoration.setBoundsToSet(expectedBounds);

        mDecoration.onDrawOver(mCanvas, mRecyclerView, mState);

        verify(mCanvas)
                .drawRoundRect(
                        mRectCaptor.capture(),
                        eq(mThickness / 2.0f),
                        eq(mThickness / 2.0f),
                        mPaintCaptor.capture());

        RectF drawnRect = mRectCaptor.getValue();
        assertEquals(expectedBounds.left, drawnRect.left, 0.01f);
        assertEquals(expectedBounds.top, drawnRect.top, 0.01f);
        assertEquals(expectedBounds.right, drawnRect.right, 0.01f);
        assertEquals(expectedBounds.bottom, drawnRect.bottom, 0.01f);

        Paint paint = mPaintCaptor.getValue();
        assertEquals(SemanticColorUtils.getColorPrimary(mActivity), paint.getColor());
        assertEquals(Paint.Style.FILL, paint.getStyle());

        assertNotNull(mDecoration.getPaintForTesting());
        assertNotNull(mDecoration.getRectFForTesting());
    }

    @Test
    @SmallTest
    public void testGetAttachedTargetView() {
        DropTargetResult resultNullVh = createTestDropTargetResult(null);
        assertNull(mDecoration.callGetAttachedTargetView(resultNullVh, mRecyclerView));

        View unattachedView = new View(mActivity);
        RecyclerView.ViewHolder unattachedVh = new RecyclerView.ViewHolder(unattachedView) {};
        DropTargetResult resultUnattached = createTestDropTargetResult(unattachedVh);
        assertNull(mDecoration.callGetAttachedTargetView(resultUnattached, mRecyclerView));

        RecyclerView realRecyclerView = new RecyclerView(mActivity);
        realRecyclerView.setLayoutManager(
                new androidx.recyclerview.widget.LinearLayoutManager(mActivity));
        RecyclerView otherRecyclerView = new RecyclerView(mActivity);
        otherRecyclerView.setLayoutManager(
                new androidx.recyclerview.widget.LinearLayoutManager(mActivity));

        View otherView = new View(mActivity);
        otherRecyclerView.addView(otherView);
        RecyclerView.ViewHolder otherVh = new RecyclerView.ViewHolder(otherView) {};
        DropTargetResult resultOther = createTestDropTargetResult(otherVh);
        assertNull(mDecoration.callGetAttachedTargetView(resultOther, realRecyclerView));

        View attachedView = new View(mActivity);
        realRecyclerView.addView(attachedView);
        RecyclerView.ViewHolder attachedVh = new RecyclerView.ViewHolder(attachedView) {};
        DropTargetResult resultAttached = createTestDropTargetResult(attachedVh);
        assertSame(
                attachedView,
                mDecoration.callGetAttachedTargetView(resultAttached, realRecyclerView));
    }
}
