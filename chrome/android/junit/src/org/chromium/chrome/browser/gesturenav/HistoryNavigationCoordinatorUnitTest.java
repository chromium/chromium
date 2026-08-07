// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate.ActionType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.insets.InsetObserver;

@RunWith(BaseRobolectricTestRunner.class)
public class HistoryNavigationCoordinatorUnitTest {
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ViewGroup mParentView;
    @Mock private TouchEventProvider mTouchEventProvider;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private InsetObserver mInsetObserver;
    @Mock private BackActionDelegate mBackActionDelegate;
    @Mock private Tab mTab;
    @Mock private GestureNavigationUtils.Natives mGestureNavigationUtilsJni;

    @Captor private ArgumentCaptor<FullscreenManager.Observer> mFullscreenObserverCaptor;

    @Before
    public void setup() {
        GestureNavigationUtilsJni.setInstanceForTesting(mGestureNavigationUtilsJni);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        when(mParentView.getContext()).thenReturn(activity);
    }

    private void initializeHistoryNavigationCoordinator() {
        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        null,
                        mLifecycleDispatcher,
                        mParentView,
                        null,
                        ObservableSuppliers.alwaysNull(),
                        mInsetObserver,
                        null,
                        mTouchEventProvider,
                        mFullscreenManager);
    }

    @Test
    @DisabledTest // This needs to be re-worked for Q.
    public void testFullscreenObserver_onEnterAndOnExit() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        initializeHistoryNavigationCoordinator();
        verify(mFullscreenManager).addObserver(mFullscreenObserverCaptor.capture());
        NavigationHandler navigationHandler =
                mHistoryNavigationCoordinator.getNavigationHandlerForTesting();

        mFullscreenObserverCaptor.getValue().onEnterFullscreen(null, null);
        verify(mTouchEventProvider).removeTouchEventObserver(navigationHandler);
        mFullscreenObserverCaptor.getValue().onExitFullscreen(null);
        verify(mTouchEventProvider).addTouchEventObserver(navigationHandler);
    }

    @Test
    public void testWindowResizing_stopsOnScroll() {
        initializeHistoryNavigationCoordinator();
        mHistoryNavigationCoordinator.initNavigationHandler();
        NavigationHandler navigationHandler =
                mHistoryNavigationCoordinator.getNavigationHandlerForTesting();

        when(mParentView.getWidth()).thenReturn(100);
        when(mParentView.getHeight()).thenReturn(200);
        navigationHandler.onDown();

        // Simulate resizing the window.
        when(mParentView.getWidth()).thenReturn(150);
        when(mParentView.getHeight()).thenReturn(200);

        boolean handled = navigationHandler.onScroll(0f, 10f, 0f, 10f, 0f);
        assertTrue(handled);
    }

    @Test
    public void testTriggerUi_actionNone_isNoOp() {
        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        null,
                        mLifecycleDispatcher,
                        mParentView,
                        null,
                        ObservableSuppliers.createNullable(mTab),
                        mInsetObserver,
                        mBackActionDelegate,
                        mTouchEventProvider,
                        mFullscreenManager);
        mHistoryNavigationCoordinator.initNavigationHandler();
        NavigationHandler navigationHandler =
                mHistoryNavigationCoordinator.getNavigationHandlerForTesting();

        when(mTab.isDestroyed()).thenReturn(false);
        navigationHandler.setTab(mTab);
        when(mBackActionDelegate.getBackActionType(mTab)).thenReturn(ActionType.NONE);

        try (HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.IncorrectEdgeSwipe", BackGestureEventSwipeEdge.LEFT)) {
            boolean triggered =
                    navigationHandler.triggerUi(
                            BackGestureEventSwipeEdge.LEFT,
                            NavigationHandler.TriggerUiCallSource.WEBPAGE_OVERSCROLL);
            assertTrue(triggered);
            assertFalse(navigationHandler.isActive());
            verify(mBackActionDelegate).onGestureUnhandled();
        }
    }

    @Test
    public void testTriggerUi_forwardSwipe_recordsIncorrectEdgeSwipeEvenIfBackActionNone() {
        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        null,
                        mLifecycleDispatcher,
                        mParentView,
                        null,
                        ObservableSuppliers.createNullable(mTab),
                        mInsetObserver,
                        mBackActionDelegate,
                        mTouchEventProvider,
                        mFullscreenManager);
        mHistoryNavigationCoordinator.initNavigationHandler();
        NavigationHandler navigationHandler =
                mHistoryNavigationCoordinator.getNavigationHandlerForTesting();

        when(mTab.isDestroyed()).thenReturn(false);
        navigationHandler.setTab(mTab);
        when(mTab.canGoForward()).thenReturn(false);
        when(mBackActionDelegate.getBackActionType(mTab)).thenReturn(ActionType.NONE);

        try (HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.IncorrectEdgeSwipe", BackGestureEventSwipeEdge.RIGHT)) {
            boolean triggered =
                    navigationHandler.triggerUi(
                            BackGestureEventSwipeEdge.RIGHT,
                            NavigationHandler.TriggerUiCallSource.WEBPAGE_OVERSCROLL);
            assertTrue(triggered);
            assertFalse(navigationHandler.isActive());
            verify(mBackActionDelegate).onGestureUnhandled();
        }
    }
}
