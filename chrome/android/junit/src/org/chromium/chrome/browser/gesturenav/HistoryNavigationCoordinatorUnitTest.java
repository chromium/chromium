// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

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

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class HistoryNavigationCoordinatorUnitTest {
    private HistoryNavigationCoordinator mHistoryNavigationCoordinator;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ViewGroup mParentView;
    @Mock private ActivityTabProvider mTab;
    @Mock private Supplier<TouchEventProvider> mTouchEventProviderSupplier;
    @Mock private TouchEventProvider mTouchEventProvider;
    @Mock private FullscreenManager mFullscreenManager;

    @Captor private ArgumentCaptor<FullscreenManager.Observer> mFullscreenObserverCaptor;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        when(mParentView.getContext()).thenReturn(activity);
        when(mTouchEventProviderSupplier.get()).thenReturn(mTouchEventProvider);
    }

    private void initializeHistoryNavigationCoordinator() {
        mHistoryNavigationCoordinator =
                HistoryNavigationCoordinator.create(
                        null,
                        mLifecycleDispatcher,
                        mParentView,
                        null,
                        mTab,
                        null,
                        null,
                        mTouchEventProviderSupplier,
                        mFullscreenManager);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.AUTOMOTIVE_FULLSCREEN_TOOLBAR_IMPROVEMENTS})
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
}
