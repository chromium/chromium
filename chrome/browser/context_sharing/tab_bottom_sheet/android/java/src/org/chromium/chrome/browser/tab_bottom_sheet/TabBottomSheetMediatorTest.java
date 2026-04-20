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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
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
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testUpdateResizingState_BelowDefaultHeight() {
        float heightFraction = DEFAULT_HEIGHT_RATIO - 0.1f;
        int offsetHeight = (int) (MAX_OFFSET * heightFraction);

        mMediator.updateResizingState(
                DEFAULT_HEIGHT_RATIO, heightFraction, offsetHeight, MAX_OFFSET);

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        assertEquals((int) (MAX_OFFSET * DEFAULT_HEIGHT_RATIO), state.webUiContainerHeight);
        assertEquals(heightFraction, state.heightFraction, EPSILON);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testUpdateResizingState_AboveDefaultHeight() {
        float heightFraction = DEFAULT_HEIGHT_RATIO + 0.1f;
        int offsetHeight = (int) (MAX_OFFSET * heightFraction);

        mMediator.updateResizingState(
                DEFAULT_HEIGHT_RATIO, heightFraction, offsetHeight, MAX_OFFSET);

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        assertEquals(offsetHeight, state.webUiContainerHeight);
        assertEquals(heightFraction, state.heightFraction, EPSILON);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testUpdateResizingState_FeatureDisabled() {
        float heightFraction = DEFAULT_HEIGHT_RATIO + 0.1f;
        int offsetHeight = (int) (MAX_OFFSET * heightFraction);

        mMediator.updateResizingState(
                DEFAULT_HEIGHT_RATIO, heightFraction, offsetHeight, MAX_OFFSET);

        ResizingState state = mModel.get(TabBottomSheetProperties.RESIZING_STATE);
        assertEquals(MAX_OFFSET, state.webUiContainerHeight);
        assertEquals(1.0f, state.heightFraction, EPSILON);
    }
}
