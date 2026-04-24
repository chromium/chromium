// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewParent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetMediatorTest {
    private static final float DEFAULT_HEIGHT_RATIO = 0.5f;
    private static final int MAX_OFFSET = 1000;
    private static final float EPSILON = 0.001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private PropertyModel mModel;
    private TabBottomSheetMediator mMediator;

    @Mock private CoBrowseViews mCoBrowseViews;
    @Mock private TabBottomSheetWebUiContainer mView;
    @Mock private ViewParent mParent;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        when(mCoBrowseViews.getView()).thenReturn(mView);
        when(mView.getContext()).thenReturn(mContext);
        when(mView.getParent()).thenReturn(mParent);

        mModel = TabBottomSheetProperties.createDefaultModel(mCoBrowseViews);
        mMediator = new TabBottomSheetMediator(mContext, mModel, mCoBrowseViews, 0.7f, 0.9f);
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_Full() {
        mMediator.onSheetStateChanged(BottomSheetController.SheetState.FULL);
        assertEquals(BottomSheetController.SheetState.FULL, mMediator.getSheetStateForTesting());
        assertEquals(0.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_Peek() {
        mMediator.onSheetStateChanged(BottomSheetController.SheetState.PEEK);
        assertEquals(BottomSheetController.SheetState.PEEK, mMediator.getSheetStateForTesting());
        assertEquals(1.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_Half() {
        mMediator.onSheetStateChanged(BottomSheetController.SheetState.HALF);
        assertEquals(BottomSheetController.SheetState.HALF, mMediator.getSheetStateForTesting());
        assertEquals(0.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_AtPeek() {
        int peekHeight = 100;

        mMediator.setPeekHeight(peekHeight);
        mMediator.updateCrossFadeAlpha(peekHeight);

        assertEquals(1.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_Transition() {
        int peekHeight = 100;
        float offsetPx = 150f;

        mMediator.setPeekHeight(peekHeight);
        mMediator.updateCrossFadeAlpha(offsetPx);

        assertEquals(0.5f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_AtDoublePeek() {
        int peekHeight = 100;
        float offsetPx = 200f;

        mMediator.setPeekHeight(peekHeight);
        mMediator.updateCrossFadeAlpha(offsetPx);

        assertEquals(0.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_PeekHeightZero() {
        mMediator.setPeekHeight(0);
        mMediator.updateCrossFadeAlpha(100);

        assertEquals(0.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_BelowPeek() {
        int peekHeight = 100;
        float offsetPx = 50f;

        mMediator.setPeekHeight(peekHeight);
        mMediator.updateCrossFadeAlpha(offsetPx);

        assertEquals(1.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testUpdateCrossFadeAlpha_AboveDoublePeek() {
        int peekHeight = 100;
        float offsetPx = 250f;

        mMediator.setPeekHeight(peekHeight);
        mMediator.updateCrossFadeAlpha(offsetPx);

        assertEquals(0.0f, mModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA), EPSILON);
    }

    @Test
    @SmallTest
    public void testIsSheetHeightSufficient_Sufficient() {
        float density = DisplayAndroid.getNonMultiDisplay(mContext).getDipScale();
        int sufficientPx = (int) Math.ceil(240 * density);
        Assert.assertTrue(mMediator.isSheetHeightSufficient(sufficientPx));
    }

    @Test
    @SmallTest
    public void testIsSheetHeightSufficient_Insufficient() {
        float density = DisplayAndroid.getNonMultiDisplay(mContext).getDipScale();
        int insufficientPx = (int) (239 * density);
        Assert.assertFalse(mMediator.isSheetHeightSufficient(insufficientPx));
    }

    @Test
    public void testGetWebUiTouchHandler() {
        Assert.assertNotNull(mMediator.getWebUiTouchHandler());
    }

    @Test
    public void testDispatchToContent() {
        mMediator.onSheetStateChanged(SheetState.FULL);
        mMediator.setPeekHeight(100); // Gesture zone max(100, 48) = 100
        MotionEvent down = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        boolean handled = mMediator.getWebUiTouchHandler().handleTouchEvent(mView, down);

        Assert.assertTrue(
                "Should be dispatched to content since it is below gesture zone", handled);
        verify(mView, atLeastOnce()).dispatchTouchEvent(any(MotionEvent.class));
        verify(mParent, atLeastOnce()).requestDisallowInterceptTouchEvent(true);
    }

    @Test
    public void testInterceptBySheetWhenNotMaximized() {
        mMediator.onSheetStateChanged(SheetState.PEEK);
        mMediator.setPeekHeight(100); // Gesture zone max(100, 48) = 100
        MotionEvent down =
                MotionEvent.obtain(
                        0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0); // Below gesture zone
        boolean handled = mMediator.getWebUiTouchHandler().handleTouchEvent(mView, down);

        Assert.assertFalse("Should fallback to sheet since it is not maximized", handled);
        verify(mParent, atLeastOnce()).requestDisallowInterceptTouchEvent(false);
    }

    @Test
    public void testInterceptBySheetInGestureZone() {
        mMediator.onSheetStateChanged(SheetState.FULL);
        mMediator.setPeekHeight(100); // Gesture zone max(100, 48) = 100

        // 1. ACTION_DOWN inside the gesture zone (Y = 50)
        MotionEvent down = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 50, 0);
        boolean handled = mMediator.getWebUiTouchHandler().handleTouchEvent(mView, down);

        Assert.assertFalse("Should fallback to sheet since it is in gesture zone", handled);
        verify(mParent, atLeastOnce()).requestDisallowInterceptTouchEvent(false);
    }

    @Test
    public void testTouchArbitrator_SheetHidden() {
        mMediator.onSheetStateChanged(SheetState.HIDDEN);
        MotionEvent down = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 50, 0);
        boolean handled = mMediator.getWebUiTouchHandler().handleTouchEvent(mView, down);

        Assert.assertFalse("Should return false immediately when sheet is hidden", handled);
    }

    @Test
    public void testTouchArbitrator_SmallPeekHeight() {
        mMediator.onSheetStateChanged(SheetState.FULL);
        mMediator.setPeekHeight(10); // Small peek height

        int minTouchTargetPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        // Gesture zone should be minTouchTargetPx (since 10 < minTouchTargetPx)

        // Touch inside gesture zone
        MotionEvent downInside =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, minTouchTargetPx - 1, 0);
        boolean handledInside =
                mMediator.getWebUiTouchHandler().handleTouchEvent(mView, downInside);
        Assert.assertFalse("Should fallback to sheet inside gesture zone", handledInside);

        // Touch outside gesture zone
        MotionEvent downOutside =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, minTouchTargetPx + 1, 0);
        boolean handledOutside =
                mMediator.getWebUiTouchHandler().handleTouchEvent(mView, downOutside);
        Assert.assertTrue("Should be dispatched to content outside gesture zone", handledOutside);
    }

    @Test
    public void testIsMaximized() {
        mMediator.onSheetStateChanged(SheetState.PEEK);
        Assert.assertFalse(mMediator.isMaximized());

        mMediator.onSheetStateChanged(SheetState.FULL);
        Assert.assertTrue(mMediator.isMaximized());
    }

    @Test
    @SmallTest
    public void testSetToFlexibleHeight() {
        mMediator.setToFlexibleHeight();

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        Assert.assertFalse(state.atFixedHeight);
        assertEquals(-1, state.webUiContainerHeight);
    }

    @Test
    @SmallTest
    public void testSetToFixedHeight() {
        mMediator.setToFixedHeight(MAX_OFFSET);

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        Assert.assertTrue(state.atFixedHeight);
        assertEquals(MAX_OFFSET, state.webUiContainerHeight);
    }

    @Test
    @SmallTest
    public void testOnSheetResizingStatusChanged() {
        mMediator.onSheetResizingStatusChanged(true);
        Assert.assertTrue(mModel.get(TabBottomSheetProperties.IS_RESIZING));

        mMediator.onSheetResizingStatusChanged(false);
        Assert.assertFalse(mModel.get(TabBottomSheetProperties.IS_RESIZING));
    }
}
