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
import android.view.View;
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
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetMediatorTest {
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
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.FULL, /* hasPeekView= */ true);
        assertEquals(BottomSheetController.SheetState.FULL, mMediator.getSheetStateForTesting());
        assertEquals(
                0.0f, mModel.get(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA), 0);
        assertEquals(
                View.GONE,
                (int)
                        mModel.get(
                                TabBottomSheetProperties
                                        .PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY));
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_Peek() {
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.PEEK, /* hasPeekView= */ true);

        assertEquals(BottomSheetController.SheetState.PEEK, mMediator.getSheetStateForTesting());
        assertEquals(
                1.0f, mModel.get(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA), 0);
        assertEquals(
                View.VISIBLE,
                (int)
                        mModel.get(
                                TabBottomSheetProperties
                                        .PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY));
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_Peek_NoPeekView() {
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.FULL, /* hasPeekView= */ true);
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.PEEK, /* hasPeekView= */ false);

        assertEquals(BottomSheetController.SheetState.PEEK, mMediator.getSheetStateForTesting());
        assertEquals(
                0.0f, mModel.get(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA), 0);
        assertEquals(
                View.GONE,
                (int)
                        mModel.get(
                                TabBottomSheetProperties
                                        .PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY));
    }


    @Test
    @SmallTest
    public void testOnSheetStateChanged_Half() {
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.FULL, /* hasPeekView= */ true);
        mMediator.onSheetStateChanged(
                BottomSheetController.SheetState.HALF, /* hasPeekView= */ true);

        assertEquals(BottomSheetController.SheetState.HALF, mMediator.getSheetStateForTesting());
        assertEquals(
                0.0f, mModel.get(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA), 0);
        assertEquals(
                View.GONE,
                (int)
                        mModel.get(
                                TabBottomSheetProperties
                                        .PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY));
    }

    @Test
    public void testGetWebUiTouchHandler() {
        Assert.assertNotNull(mMediator.getWebUiTouchHandler());
    }

    @Test
    public void testDispatchToContent() {
        mMediator.onSheetStateChanged(SheetState.FULL, false);
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
        mMediator.onSheetStateChanged(SheetState.PEEK, false);
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
        mMediator.onSheetStateChanged(SheetState.FULL, false);
        mMediator.setPeekHeight(100); // Gesture zone max(100, 48) = 100

        // 1. ACTION_DOWN inside the gesture zone (Y = 50)
        MotionEvent down = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 50, 0);
        boolean handled = mMediator.getWebUiTouchHandler().handleTouchEvent(mView, down);

        Assert.assertFalse("Should fallback to sheet since it is in gesture zone", handled);
        verify(mParent, atLeastOnce()).requestDisallowInterceptTouchEvent(false);
    }

    @Test
    public void testIsMaximized() {
        mMediator.onSheetStateChanged(SheetState.PEEK, false);
        Assert.assertFalse(mMediator.isMaximized());

        mMediator.onSheetStateChanged(SheetState.FULL, false);
        Assert.assertTrue(mMediator.isMaximized());
    }

    @Test
    @SmallTest
    public void testUpdateResizingState() {
        float defaultHeightRatio = mMediator.getFullHeightRatioForTesting();
        float heightFraction = defaultHeightRatio + 0.1f;
        int maxOffset = 1000;
        int offsetHeight = (int) (maxOffset * heightFraction);

        mMediator.updateResizingState(
                defaultHeightRatio, heightFraction, offsetHeight, maxOffset);

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        assertEquals(maxOffset, state.webUiContainerHeight);
        assertEquals(1.0f, state.heightFraction, 0.01f);
    }
}
