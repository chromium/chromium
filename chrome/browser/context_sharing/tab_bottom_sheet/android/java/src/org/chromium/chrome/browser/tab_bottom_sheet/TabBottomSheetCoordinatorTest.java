// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetCoordinator.SheetEventsCallback;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetCoordinatorTest {
    private static final float FULL_HEIGHT_RATIO = 0.7f;
    private static final float SMALL_SCREEN_HEIGHT_RATIO = 0.9f;
    private static final int MAX_OFFSET = 1000;
    private static final int CONTAINER_WIDTH = 500;
    private static final int CONTAINER_HEIGHT = 500;
    private static final int INSUFFICIENT_CONTAINER_HEIGHT = 100;
    private static final int LARGE_SCROLL_DP = 120;
    private static final int LARGE_FLING_DP = 60;
    private static final int SMALL_SCROLL_DP = 40;
    private static final int SMALL_FLING_DP = 5;
    private static final float HALF_HEIGHT_FRACTION = 0.5f;
    private static final float HALF_OFFSET_HEIGHT = 500f;
    private static final float FULL_HEIGHT_FRACTION = 1.0f;
    private static final float EPSILON = 0.001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final SheetEventsCallback mSheetEventsCallback =
            new SheetEventsCallback() {
                @Override
                public void onBottomSheetClosed() {}

                @Override
                public void onBottomSheetOpened(boolean isExpanded) {}
            };

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;
    @Mock private TouchEventProvider mMockTouchEventProvider;
    @Mock private CoBrowseViews mCoBrowseViews;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Captor private ArgumentCaptor<TabBottomSheetContent> mBottomSheetContentArgumentCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverArgumentCaptor;
    @Captor private ArgumentCaptor<ComponentCallbacks> mComponentCallbacksArgumentCaptor;
    @Captor private ArgumentCaptor<TouchEventObserver> mTouchEventObserverArgumentCaptor;

    private Context mContext;
    private View mView;
    private TabBottomSheetCoordinator mCoordinator;
    private PropertyModel mCoordinatorModel;

    @Before
    public void setUp() {
        mContext = spy(ApplicationProvider.getApplicationContext());
        mView = new FrameLayout(mContext);
        when(mCoBrowseViews.getView()).thenReturn(mView);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);

        when(mWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockDecorView.getHeight()).thenReturn(MAX_OFFSET);
        doAnswer(
                        invocation -> {
                            Rect rect = invocation.getArgument(0);
                            rect.set(0, 0, CONTAINER_WIDTH, MAX_OFFSET);
                            return null;
                        })
                .when(mMockDecorView)
                .getWindowVisibleDisplayFrame(any(Rect.class));

        mCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mMockBottomSheetController,
                        mMockTouchEventProvider,
                        mCoBrowseViews,
                        mSheetEventsCallback);

        mCoordinatorModel = mCoordinator.getModelForTesting();
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    /**
     * Helper to simulate a successful request to show content and get the Coordinator's observer.
     *
     * @return The BottomSheetObserver instance used by the Coordinator.
     */
    private BottomSheetObserver simulateShowSuccessAndGetObserver() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenAnswer(
                        invocation -> {
                            BottomSheetContent content = invocation.getArgument(0);
                            when(mMockBottomSheetController.getCurrentSheetContent())
                                    .thenReturn(content);
                            return true;
                        });
        mCoordinator.tryToShowBottomSheet(/* animate= */ true, /* startsExpanded= */ true);
        when(mMockBottomSheetController.getCurrentSheetContent())
                .thenReturn(mCoordinator.getSheetContentForTesting());
        verify(mMockBottomSheetController)
                .addObserver(mBottomSheetObserverArgumentCaptor.capture());
        BottomSheetObserver coordinatorObserver = mBottomSheetObserverArgumentCaptor.getValue();
        assertNotNull(
                "Coordinator's observer should be set after successful show.", coordinatorObserver);
        verify(mMockBottomSheetController).addObserver(eq(coordinatorObserver));
        doAnswer(
                        invocation -> {
                            coordinatorObserver.onSheetStateChanged(
                                    SheetState.HIDDEN, StateChangeReason.NONE);
                            return null;
                        })
                .when(mMockBottomSheetController)
                .hideContent(any(), anyBoolean(), anyInt());
        return coordinatorObserver;
    }

    @Test
    public void testShowBottomSheet_Success_ShowsAndObserves() {
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        assertNotNull(mBottomSheetContentArgumentCaptor.getValue());
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testShowBottomSheet_Fails_Cleanup() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.tryToShowBottomSheet(/* animate= */ true, /* startsExpanded= */ true);
        verify(mMockBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), eq(true));
        verify(mMockBottomSheetController, never()).addObserver(any(BottomSheetObserver.class));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testDestroy_WhenShown_HidesAndCleansUp() {
        simulateShowSuccessAndGetObserver();
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
        mCoordinator.destroy();

        verify(mMockBottomSheetController)
                .hideContent(
                        any(TabBottomSheetContent.class), eq(false), eq(StateChangeReason.NONE));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testDestroy_WhenNotShown_CleansUp() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.tryToShowBottomSheet(/* animate= */ true, /* startsExpanded= */ true);
        mCoordinator.destroy();

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testShowBottomSheet_ContentHasCustomLifecycle() {
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        TabBottomSheetContent content = mBottomSheetContentArgumentCaptor.getValue();
        assertNotNull(content);
        assertTrue(content.hasCustomLifecycle());
    }

    @Test
    public void testCorrectFullHeightRatio_WithoutKeyboard() {
        when(mKeyboardDelegate.isKeyboardShowing(eq(mView))).thenReturn(false);
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        TabBottomSheetContent content = mBottomSheetContentArgumentCaptor.getValue();
        assertNotNull(content);
        assertEquals(HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), EPSILON);
    }

    @Test
    public void testCorrectFullHeightRatio_WithKeyboard() {
        when(mKeyboardDelegate.isKeyboardShowing(eq(mView))).thenReturn(true);
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        TabBottomSheetContent content = mBottomSheetContentArgumentCaptor.getValue();
        assertNotNull(content);
        assertEquals(HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), EPSILON);
    }

    @Test
    public void testComponentCallbacksRegistration() {
        simulateShowSuccessAndGetObserver();
        verify(mContext).registerComponentCallbacks(any(ComponentCallbacks.class));

        mCoordinator.destroy();
        verify(mContext).unregisterComponentCallbacks(any(ComponentCallbacks.class));
    }

    @Test
    public void testDoNotExpandWhenInsufficientSpace() {
        when(mMockBottomSheetController.getContainerHeight())
                .thenReturn(INSUFFICIENT_CONTAINER_HEIGHT);
        simulateShowSuccessAndGetObserver();

        verify(mMockBottomSheetController, never()).expandSheet();
    }

    @Test
    public void testOnConfigurationChanged() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        verify(mContext).registerComponentCallbacks(mComponentCallbacksArgumentCaptor.capture());
        ComponentCallbacks callbacks = mComponentCallbacksArgumentCaptor.getValue();

        // Simulate configuration change
        callbacks.onConfigurationChanged(new Configuration());

        assertTrue(mCoordinator.isExpectingLayoutChangeForTesting());

        // Trigger the layout changed
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        verify(mMockBottomSheetController).collapseSheet(true);
        assertFalse(mCoordinator.isExpectingLayoutChangeForTesting());
    }

    @Test
    public void testTouchEventObserverRegistration() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // State HALF should add observer
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        verify(mMockTouchEventProvider)
                .addTouchEventObserver(mTouchEventObserverArgumentCaptor.capture());
        TouchEventObserver touchEventObserver = mTouchEventObserverArgumentCaptor.getValue();
        assertNotNull(touchEventObserver);

        // State HIDDEN should remove observer
        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        verify(mMockTouchEventProvider, atLeastOnce())
                .removeTouchEventObserver(eq(touchEventObserver));
    }

    @Test
    public void testTouchEventObserver_OnInterceptTouchEvent() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // State FULL should add observer
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        verify(mMockTouchEventProvider)
                .addTouchEventObserver(mTouchEventObserverArgumentCaptor.capture());
        TouchEventObserver touchEventObserver = mTouchEventObserverArgumentCaptor.getValue();

        // Passing event should not crash and should return false
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertFalse(touchEventObserver.onInterceptTouchEvent(event));
    }

    @Test
    public void testGestureListener_Scroll() {
        simulateShowSuccessAndGetObserver();

        GestureDetector.SimpleOnGestureListener listener =
                mCoordinator.getGestureListenerForTesting();

        MotionEvent e1 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent e2Small =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_MOVE,
                        0,
                        ViewUtils.dpToPx(mContext, SMALL_SCROLL_DP),
                        0);
        MotionEvent e2Large =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_MOVE,
                        0,
                        ViewUtils.dpToPx(mContext, LARGE_SCROLL_DP),
                        0);

        // Small scroll should not collapse
        listener.onScroll(e1, e2Small, 0, SMALL_SCROLL_DP);
        verify(mMockBottomSheetController, never()).collapseSheet(true);

        // Large scroll should collapse
        listener.onScroll(e1, e2Large, 0, LARGE_SCROLL_DP);
        verify(mMockBottomSheetController).collapseSheet(true);
    }

    @Test
    public void testGestureListener_Fling() {
        simulateShowSuccessAndGetObserver();

        GestureDetector.SimpleOnGestureListener listener =
                mCoordinator.getGestureListenerForTesting();

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        // Small fling should not collapse
        listener.onFling(event, event, 0, ViewUtils.dpToPx(mContext, SMALL_FLING_DP));
        verify(mMockBottomSheetController, never()).collapseSheet(true);

        // Large fling should collapse
        listener.onFling(event, event, 0, ViewUtils.dpToPx(mContext, LARGE_FLING_DP));
        verify(mMockBottomSheetController).collapseSheet(true);
    }

    @Test
    public void testGestureListener_DoubleTapAndLongPress() {
        simulateShowSuccessAndGetObserver();

        GestureDetector.SimpleOnGestureListener listener =
                mCoordinator.getGestureListenerForTesting();

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        listener.onDoubleTap(event);
        verify(mMockBottomSheetController).collapseSheet(true);

        listener.onLongPress(event);
        // Verify it was called twice now
        verify(mMockBottomSheetController, times(2)).collapseSheet(true);
    }

    @Test
    public void testOnConfigurationChanged_whileNotManaged() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        verify(mContext).registerComponentCallbacks(mComponentCallbacksArgumentCaptor.capture());
        ComponentCallbacks callbacks = mComponentCallbacksArgumentCaptor.getValue();

        // Destroy the coordinator so it's no longer managed
        mCoordinator.destroy();

        // Configuration change should not run its internal logic
        callbacks.onConfigurationChanged(new Configuration());

        assertFalse(mCoordinator.isExpectingLayoutChangeForTesting());

        // Try invoking layout change, nothing should happen
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        // Collapse sheet should NOT be called after destruction (aside from the one in destroy())
        // In destroy(), hideContent is called, but collapseSheet is what the runnable does.
        verify(mMockBottomSheetController, never()).collapseSheet(anyBoolean());
        assertFalse(mCoordinator.isExpectingLayoutChangeForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnContainerSizeChanged() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        int viewportHeight = 999;
        doAnswer(
                        invocation -> {
                            Rect rect = invocation.getArgument(0);
                            rect.set(0, 0, CONTAINER_WIDTH, viewportHeight);
                            return null;
                        })
                .when(mMockDecorView)
                .getWindowVisibleDisplayFrame(any(Rect.class));

        observer.onContainerSizeChanged(CONTAINER_WIDTH, viewportHeight);

        ResizingState state = mCoordinatorModel.get(TabBottomSheetProperties.RESIZING_STATE);
        int expectedWebUiHeight = (int) (viewportHeight * FULL_HEIGHT_RATIO);
        assertEquals(expectedWebUiHeight, state.webUiContainerHeight);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnContainerSizeChanged_resizingEnabled() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        ResizingState impossibleState = new ResizingState(-1, -1.0f);
        mCoordinatorModel.set(TabBottomSheetProperties.RESIZING_STATE, impossibleState);
        when(mMockBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET);

        observer.onContainerSizeChanged(CONTAINER_WIDTH, MAX_OFFSET);

        // Verify that the resizing state is not updated.
        assertEquals(
                impossibleState, mCoordinatorModel.get(TabBottomSheetProperties.RESIZING_STATE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnSheetOffsetChanged_resizingEnabled_withoutKeyboard() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        when(mKeyboardDelegate.isKeyboardShowing(eq(mView))).thenReturn(false);
        when(mMockBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET);

        observer.onSheetOffsetChanged(HALF_HEIGHT_FRACTION, HALF_OFFSET_HEIGHT);

        ResizingState state = mCoordinatorModel.get(TabBottomSheetProperties.RESIZING_STATE);
        int expectedLockedHeight = Math.round(MAX_OFFSET * FULL_HEIGHT_RATIO);
        assertEquals(expectedLockedHeight, state.webUiContainerHeight);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnSheetOffsetChanged_resizingEnabled_withKeyboard() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        when(mKeyboardDelegate.isKeyboardShowing(eq(mView))).thenReturn(true);
        when(mMockBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET);

        observer.onSheetOffsetChanged(FULL_HEIGHT_FRACTION, MAX_OFFSET);

        ResizingState state = mCoordinatorModel.get(TabBottomSheetProperties.RESIZING_STATE);
        assertEquals(MAX_OFFSET, state.webUiContainerHeight);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnSheetOffsetChanged_resizingDisabled() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        ResizingState impossibleState = new ResizingState(-1, -1.0f);
        mCoordinatorModel.set(TabBottomSheetProperties.RESIZING_STATE, impossibleState);
        when(mMockBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET);

        observer.onSheetOffsetChanged(FULL_HEIGHT_FRACTION, MAX_OFFSET);

        // Verify that the resizing state is not updated.
        assertEquals(
                impossibleState, mCoordinatorModel.get(TabBottomSheetProperties.RESIZING_STATE));
    }
}
