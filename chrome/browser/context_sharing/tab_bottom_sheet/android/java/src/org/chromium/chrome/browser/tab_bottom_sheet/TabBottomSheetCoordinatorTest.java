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
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetCoordinator.SheetEventsCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
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

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SheetEventsCallback mMockSheetEventsCallback;

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;
    @Mock private TouchEventProvider mMockTouchEventProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private TabBottomSheetWebUi mMockWebUi;
    @Mock private TabBottomSheetContentProvider mMockContentProvider;

    @Captor private ArgumentCaptor<TabBottomSheetContent> mBottomSheetContentArgumentCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverArgumentCaptor;
    @Captor private ArgumentCaptor<ComponentCallbacks> mComponentCallbacksArgumentCaptor;
    @Captor private ArgumentCaptor<TouchEventObserver> mTouchEventObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<KeyboardVisibilityDelegate.KeyboardVisibilityListener>
            mKeyboardVisibilityListenerCaptor;

    @Mock private ModalDialogManager mMockModalDialogManager;
    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;

    private CoBrowseViews mCoBrowseViews;
    private Context mContext;
    private View mView;
    private TabBottomSheetCoordinator mCoordinator;
    private PropertyModel mCoordinatorModel;
    private WebViewResizingHelper mWebViewResizingHelper;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = spy(activity));
        View containerView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        containerView.setFocusable(true);
        containerView.setFocusableInTouchMode(true);

        View containerViewSpy = spy(containerView);
        // Delegate post() directly to the main looper Handler. Otherwise, because containerViewSpy
        // is a Mockito spy, calling post() delegates to the unattached containerView delegate
        // instance, which buffers the runnable indefinitely since it never gets attached to a
        // window.
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            return new Handler(Looper.getMainLooper()).post(runnable);
                        })
                .when(containerViewSpy)
                .post(any(Runnable.class));
        mWebViewResizingHelper =
                new WebViewResizingHelper(containerViewSpy, mWindowAndroid, Color.WHITE);
        when(mMockWebUi.getWebViewResizingHelper()).thenReturn(mWebViewResizingHelper);
        View webUiView = new View(mContext);
        when(mMockWebUi.getWebUiView()).thenReturn(webUiView);

        doAnswer(
                        invocation -> {
                            View view = invocation.getArgument(0);
                            float heightRatio = invocation.getArgument(1);
                            int bgColor = invocation.getArgument(2);
                            int peekViewHeight = invocation.getArgument(3);
                            int actorControlContainerId = invocation.getArgument(4);
                            int emptyPlaceholderContainerId = invocation.getArgument(5);
                            return new TestTabBottomSheetContent(
                                    view,
                                    heightRatio,
                                    bgColor,
                                    peekViewHeight,
                                    actorControlContainerId,
                                    emptyPlaceholderContainerId);
                        })
                .when(mMockContentProvider)
                .create(any(View.class), anyFloat(), anyInt(), anyInt(), anyInt(), anyInt());

        mCoBrowseViews =
                spy(
                        new CoBrowseViews(
                                containerViewSpy,
                                TabBottomSheetClientType.UNKNOWN,
                                CoBrowseContainerType.BOTTOM_SHEET,
                                mMockWebUi,
                                null,
                                0,
                                mMockContentProvider));
        mView = containerViewSpy;
        assertNotNull(
                "peek_view_container should be found in CoBrowseViews",
                mView.findViewById(R.id.peek_view_container));
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mMockModalDialogManager);

        when(mWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockDecorView.getHeight()).thenReturn(MAX_OFFSET);
        when(mMockBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET);
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
                        mMockSheetEventsCallback);

        mCoordinatorModel = mCoordinator.getModelForTesting();
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        TestTabBottomSheetContent.setUsePlaceholderForTesting(false);
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
        mActivityScenarioRule.getScenario().onActivity(activity -> activity.setContentView(mView));
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
    public void testShowBottomSheet_MultipleCalls_DoesNotAddMultipleObservers() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController, times(1)).addObserver(any(BottomSheetObserver.class));

        // Simulate suppression by setting mIsShowingTabBottomSheet to false via callback.
        observer.onSheetContentChanged(mock(BottomSheetContent.class));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());

        // Try to show again.
        mCoordinator.tryToShowBottomSheet(/* animate= */ true, /* startsExpanded= */ true);

        // Verify addObserver was not called again (total times should still be 1).
        verify(mMockBottomSheetController, times(1)).addObserver(any(BottomSheetObserver.class));
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
    public void testDoNotExpandWhenInsufficientViewportSpace() {
        // Setup a very small viewport height
        when(mMockDecorView.getHeight()).thenReturn(50);
        doAnswer(
                        invocation -> {
                            Rect rect = invocation.getArgument(0);
                            rect.set(0, 0, CONTAINER_WIDTH, 50);
                            return null;
                        })
                .when(mMockDecorView)
                .getWindowVisibleDisplayFrame(any(Rect.class));

        simulateShowSuccessAndGetObserver();

        // Flush the looper to run the runnable posted in tryToShowBottomSheet()
        ShadowLooper.idleMainLooper();

        // expandSheet(true) should NOT have been called because viewport height is insufficient
        verify(mMockBottomSheetController, never()).expandSheet(anyBoolean());
        verify(mMockSheetEventsCallback).onBottomSheetOpened(false);
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
    public void testAllowFullscreenImeOnLandscape() {
        simulateShowSuccessAndGetObserver();

        verify(mContext).registerComponentCallbacks(mComponentCallbacksArgumentCaptor.capture());
        ComponentCallbacks callbacks = mComponentCallbacksArgumentCaptor.getValue();

        Configuration landscapeConfig = new Configuration();
        landscapeConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;
        callbacks.onConfigurationChanged(landscapeConfig);

        verify(mMockWebUi).setAllowFullscreenIme(true);

        Configuration portraitConfig = new Configuration();
        portraitConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        callbacks.onConfigurationChanged(portraitConfig);

        verify(mMockWebUi).setAllowFullscreenIme(false);
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
    public void testOnlyUpdateAlphaWhenScrolling() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.PEEK);

        float sentinelAlpha = 0.7f;
        mCoordinatorModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, sentinelAlpha);

        observer.onSheetOffsetChanged(0.5f, 500f);

        assertEquals(
                sentinelAlpha,
                mCoordinatorModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA),
                EPSILON);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.SCROLLING);

        observer.onSheetOffsetChanged(0.5f, 600f);
        assertTrue(
                sentinelAlpha != mCoordinatorModel.get(TabBottomSheetProperties.PEEK_STATE_ALPHA));
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
    public void testOnSheetStateChanged_Peek_recordsMetric() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        UserActionTester userActionTester = new UserActionTester();
        doReturn(TabBottomSheetClientType.GLIC).when(mCoBrowseViews).getClientType();
        try {
            observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
            assertEquals(1, userActionTester.getActionCount("Glic.Instance.Show.PeekView"));
        } finally {
            userActionTester.tearDown();
        }
    }

    @Test
    public void testOnSheetStateChanged_Expanded_recordsMetric() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        UserActionTester userActionTester = new UserActionTester();
        doReturn(TabBottomSheetClientType.GLIC).when(mCoBrowseViews).getClientType();
        try {
            // 1. Transitioning from HIDDEN (non-expanded) to HALF (expanded) should record metric.
            observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
            assertEquals(1, userActionTester.getActionCount("Glic.Instance.Show.BottomSheet"));

            // 2. Transitioning between open expanded states (HALF to FULL) should NOT record metric
            // again.
            observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
            assertEquals(1, userActionTester.getActionCount("Glic.Instance.Show.BottomSheet"));

            // 3. Transitioning from FULL to PEEK (non-expanded) then back to HALF (expanded) should
            // record it a second time.
            observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
            observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
            assertEquals(2, userActionTester.getActionCount("Glic.Instance.Show.BottomSheet"));
        } finally {
            userActionTester.tearDown();
        }
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
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnContainerSizeChanged_resizingEnabled() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);
        // Resizing state is set to flexible height on the second call.
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnContainerSizeChanged_resizingDisabled() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        int expectedFixedHeight = (int) (MAX_OFFSET * FULL_HEIGHT_RATIO);
        assertEquals(expectedFixedHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnContainerSizeChanged_StartWithFixedHeight() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        int expectedFixedHeight = (int) (MAX_OFFSET * FULL_HEIGHT_RATIO);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(expectedFixedHeight);

        observer.onContainerSizeChanged(CONTAINER_WIDTH, expectedFixedHeight);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);

        assertEquals(expectedFixedHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnContainerSizeChanged_FallbackToFlexible() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        int desiredFixedHeight = (int) (MAX_OFFSET * FULL_HEIGHT_RATIO);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(desiredFixedHeight - 1);

        observer.onContainerSizeChanged(CONTAINER_WIDTH, desiredFixedHeight);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testOnContainerSizeChanged_MultipleCalls() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        int expectedFixedHeight = (int) (MAX_OFFSET * FULL_HEIGHT_RATIO);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(expectedFixedHeight);

        observer.onContainerSizeChanged(CONTAINER_WIDTH, expectedFixedHeight);
        observer.onContainerSizeChanged(CONTAINER_WIDTH, expectedFixedHeight);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/true")
    public void testFixedHeightCalculation_UsesLandscapeRatio() {
        Configuration landscapeConfig = new Configuration();
        landscapeConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        Resources resources = mock();
        when(mContext.getResources()).thenReturn(resources);
        when(resources.getConfiguration()).thenReturn(landscapeConfig);

        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        int expectedLandscapeHeight = (int) (MAX_OFFSET * SMALL_SCREEN_HEIGHT_RATIO);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(expectedLandscapeHeight);

        observer.onContainerSizeChanged(CONTAINER_WIDTH, expectedLandscapeHeight);

        View expandedContent = mView.findViewById(R.id.expanded_content_group);

        assertEquals(expectedLandscapeHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnContainerSizeChanged_ActivityPaused_DoesNotChangeHeight() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        int initialHeight = expandedContent.getLayoutParams().height;

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.PAUSED);
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(initialHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnContainerSizeChanged_ActivityStopped_DoesNotChangeHeight() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        int initialHeight = expandedContent.getLayoutParams().height;

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(initialHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET + ":resize_webview/false")
    public void testOnContainerSizeChanged_ActivityDestroyed_DoesNotChangeHeight() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View expandedContent = mView.findViewById(R.id.expanded_content_group);
        int initialHeight = expandedContent.getLayoutParams().height;

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.DESTROYED);
        observer.onContainerSizeChanged(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(initialHeight, expandedContent.getLayoutParams().height);
    }

    @Test
    public void testKeyboardOpen_FocusOutside_Collapses() {
        simulateShowSuccessAndGetObserver();

        verify(mKeyboardDelegate)
                .addKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mKeyboardVisibilityListenerCaptor.getValue();
        assertNotNull(listener);

        // Ensure focus is not in the bottom sheet.
        mView.clearFocus();

        listener.keyboardVisibilityChanged(true);

        verify(mMockBottomSheetController).collapseSheet(true);
    }

    @Test
    public void testKeyboardOpen_FocusInside_DoesNotCollapse() {
        simulateShowSuccessAndGetObserver();

        verify(mKeyboardDelegate)
                .addKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mKeyboardVisibilityListenerCaptor.getValue();
        assertNotNull(listener);

        // Request focus inside the bottom sheet.
        mView.requestFocus();

        listener.keyboardVisibilityChanged(true);

        verify(mMockBottomSheetController, never()).collapseSheet(true);
    }

    @Test
    public void testKeyboardClosed_DoesNotCollapse() {
        simulateShowSuccessAndGetObserver();

        verify(mKeyboardDelegate)
                .addKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mKeyboardVisibilityListenerCaptor.getValue();
        assertNotNull(listener);

        listener.keyboardVisibilityChanged(false);

        verify(mMockBottomSheetController, never()).collapseSheet(anyBoolean());
    }

    @Test
    public void testKeyboardVisibility_SetsIgnoreClearFocusOnWebUi() {
        simulateShowSuccessAndGetObserver();

        verify(mKeyboardDelegate)
                .addKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mKeyboardVisibilityListenerCaptor.getValue();
        assertNotNull(listener);

        listener.keyboardVisibilityChanged(true);
        verify(mMockWebUi).setIgnoreClearFocus(true);

        listener.keyboardVisibilityChanged(false);
        verify(mMockWebUi).setIgnoreClearFocus(false);
    }

    @Test
    public void testModalDialogObserver_RegistrationAndUnregistration() {
        // 1. Show sheet and verify observer registration
        simulateShowSuccessAndGetObserver();
        verify(mMockModalDialogManager).addObserver(mModalDialogManagerObserverCaptor.capture());
        ModalDialogManagerObserver observer = mModalDialogManagerObserverCaptor.getValue();
        assertNotNull(observer);

        // 2. Destroy coordinator and verify observer unregistration
        mCoordinator.destroy();
        verify(mMockModalDialogManager).removeObserver(observer);
    }

    @Test
    public void testModalDialogObserver_TabModalCollapsesSheet() {
        simulateShowSuccessAndGetObserver();
        verify(mMockModalDialogManager).addObserver(mModalDialogManagerObserverCaptor.capture());
        ModalDialogManagerObserver observer = mModalDialogManagerObserverCaptor.getValue();

        // Setup: Mock active tab-modal dialog state
        when(mMockModalDialogManager.getCurrentType()).thenReturn(ModalDialogType.TAB);

        // Trigger observer
        observer.onDialogShown(mock(View.class));

        // Verify sheet collapse is called (via bottom sheet controller)
        verify(mMockBottomSheetController).collapseSheet(eq(true));
    }

    @Test
    public void testModalDialogObserver_AppModalDoesNotCollapseSheet() {
        simulateShowSuccessAndGetObserver();
        verify(mMockModalDialogManager).addObserver(mModalDialogManagerObserverCaptor.capture());
        ModalDialogManagerObserver observer = mModalDialogManagerObserverCaptor.getValue();

        // Setup: Mock active app-modal dialog state
        when(mMockModalDialogManager.getCurrentType()).thenReturn(ModalDialogType.APP);

        // Trigger observer
        observer.onDialogShown(mock(View.class));

        // Verify collapse is NOT triggered
        verify(mMockBottomSheetController, never()).collapseSheet(anyBoolean());
    }

    @Test
    public void testMetrics_Glic_CurrentStateAndTransitions() {
        // Re-create coordinator with GLIC client type
        mCoBrowseViews =
                new CoBrowseViews(
                        mView,
                        TabBottomSheetClientType.GLIC,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mMockWebUi,
                        null,
                        0,
                        mMockContentProvider);
        mCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mMockBottomSheetController,
                        mMockTouchEventProvider,
                        mCoBrowseViews,
                        mMockSheetEventsCallback);

        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // Initial entry to PEEK state via SWIPE (1 = SWIPE)
        HistogramWatcher stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.CurrentState", 1); // 1 = PEEK
        HistogramWatcher reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.StateChangeReason.Peek", 1); // 1 = SWIPE
        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.SWIPE);
        stateWatcher.assertExpected();
        reasonWatcher.assertExpected();

        // Transition to FULL
        HistogramWatcher transitionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.StateTransition", 2); // 2 = PEEK_TO_FULL
        stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.CurrentState", 3); // 3 = FULL
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        transitionWatcher.assertExpected();
        stateWatcher.assertExpected();

        // Transition to HALF
        transitionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.StateTransition", 6); // 6 = FULL_TO_HALF
        stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.CurrentState", 2); // 2 = HALF
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        transitionWatcher.assertExpected();
        stateWatcher.assertExpected();

        // Transition to HIDDEN via BACK_PRESS (2 = BACK_PRESS)
        stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.CurrentState", 0); // 0 = HIDDEN
        reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.Glic.StateChangeReason.Hidden",
                        2); // 2 = BACK_PRESS
        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.BACK_PRESS);
        stateWatcher.assertExpected();
        reasonWatcher.assertExpected();
    }

    @Test
    public void testMetrics_ContextualTasks_CurrentStateAndTransitions() {
        // Re-create coordinator with CONTEXTUAL_TASKS client type
        mCoBrowseViews =
                new CoBrowseViews(
                        mView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        mMockWebUi,
                        null,
                        0,
                        mMockContentProvider);
        mCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mMockBottomSheetController,
                        mMockTouchEventProvider,
                        mCoBrowseViews,
                        mMockSheetEventsCallback);

        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // Initial entry to FULL state
        HistogramWatcher stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.CurrentState", 3); // 3 = FULL
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        stateWatcher.assertExpected();

        // Transition to PEEK
        HistogramWatcher transitionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.StateTransition",
                        5); // 5 = FULL_TO_PEEK
        stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.CurrentState", 1); // 1 = PEEK
        HistogramWatcher reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.StateChangeReason.Peek",
                        3); // 3 = TAP_SCRIM
        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.TAP_SCRIM);
        transitionWatcher.assertExpected();
        stateWatcher.assertExpected();
        reasonWatcher.assertExpected();

        // Close sheet via onSheetContentChanged (should record reason as NONE = 0)
        stateWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.CurrentState", 0); // 0 = HIDDEN
        reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabBottomSheet.ContextualTasks.StateChangeReason.Hidden",
                        0); // 0 = NONE
        observer.onSheetContentChanged(null);
        stateWatcher.assertExpected();
        reasonWatcher.assertExpected();
    }

    @Test
    public void testUpdateRoundingEdges_FullWidth() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        RoundedCornerOutlineProvider outlineProvider = mCoordinator.getOutlineProviderForTesting();

        when(mMockBottomSheetController.isFullWidth()).thenReturn(true);
        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        assertFalse(outlineProvider.isTopEdgeRounded());
    }

    @Test
    public void testUpdateRoundingEdges_NotFullWidth() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        RoundedCornerOutlineProvider outlineProvider = mCoordinator.getOutlineProviderForTesting();

        when(mMockBottomSheetController.isFullWidth()).thenReturn(false);
        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        assertTrue(outlineProvider.isTopEdgeRounded());
    }

    @Test
    public void testSheetEventsCallback_onBottomSheetOpened_SuppressedDuringHide() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // 1. Initial show should trigger onBottomSheetOpened(true).
        verify(mMockSheetEventsCallback).onBottomSheetOpened(true);

        // 2. Transition to hiding.
        when(mMockBottomSheetController.isSheetHiding()).thenReturn(true);

        // 3. Call onSheetStateChanged to SCROLLING (part of hiding flow).
        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);

        // 4. Verify onBottomSheetOpened was NOT called again.
        verify(mMockSheetEventsCallback, times(1)).onBottomSheetOpened(anyBoolean());
    }

    @Test
    public void testSheetEventsCallback_onBottomSheetOpened_NotSuppressedDuringNormalTransition() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // Initial show triggers onBottomSheetOpened(true).
        verify(mMockSheetEventsCallback).onBottomSheetOpened(true);

        // Transitioning to SCROLLING but NOT hiding (e.g. manual user drag or normal expansion
        // animation).
        when(mMockBottomSheetController.isSheetHiding()).thenReturn(false);
        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);

        // Verify onBottomSheetOpened was called with true (expanded).
        verify(mMockSheetEventsCallback, times(2)).onBottomSheetOpened(true);
    }

    @Test
    public void testAccessibilityFocusSentOnStableStates() {
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();

        // 1. Transition to a stable showing state (HALF)
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        // Verify accessibility focus event was sent
        verify(mView).sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);

        clearInvocations(mView);

        // 2. Transition to stable showing state (FULL)
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        // Verify it was called again (1 time after clearing)
        verify(mView).sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Test
    public void testPlaceholderVisibility_usePlaceholderFalse() {
        TestTabBottomSheetContent.setUsePlaceholderForTesting(false);
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View placeholder = mView.findViewById(R.id.empty_placeholder_container);
        assertNotNull(placeholder);

        // Initially state is HIDDEN.
        assertEquals(View.GONE, placeholder.getVisibility());

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        assertEquals(View.GONE, placeholder.getVisibility());

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        assertEquals(View.GONE, placeholder.getVisibility());
    }

    @Test
    public void testPlaceholderVisibility_usePlaceholderTrue_noWebContents() {
        TestTabBottomSheetContent.setUsePlaceholderForTesting(true);
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View placeholder = mView.findViewById(R.id.empty_placeholder_container);
        assertNotNull(placeholder);

        // Initially state is HIDDEN.
        assertEquals(View.GONE, placeholder.getVisibility());

        // Change state to HALF. Currently no WebContents (mMockWebUi.getWebContents() is null).
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.PEEK);
        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
        assertEquals(View.GONE, placeholder.getVisibility());

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        assertEquals(View.VISIBLE, placeholder.getVisibility());
    }

    @Test
    public void testPlaceholderVisibility_usePlaceholderTrue_withWebContents() {
        TestTabBottomSheetContent.setUsePlaceholderForTesting(true);
        BottomSheetObserver observer = simulateShowSuccessAndGetObserver();
        View placeholder = mView.findViewById(R.id.empty_placeholder_container);
        assertNotNull(placeholder);

        WebContents mockWebContents = mock();
        mCoBrowseViews.setWebContents(mockWebContents, false);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        assertEquals(View.GONE, placeholder.getVisibility());

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        assertEquals(View.GONE, placeholder.getVisibility());
    }
}
