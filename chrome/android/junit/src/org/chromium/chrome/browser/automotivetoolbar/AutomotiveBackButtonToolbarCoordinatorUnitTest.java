// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import static org.mockito.Mockito.verify;

import android.view.LayoutInflater;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class AutomotiveBackButtonToolbarCoordinatorUnitTest {
    private AutomotiveBackButtonToolbarCoordinator mAutomotiveBackButtonToolbarCoordinator;
    private View mAutomotiveToolbar;
    private FullscreenManager.Observer mFullscreenObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TouchEventProvider mTouchEventProvider;
    @Mock private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        View parent =
                LayoutInflater.from(activity)
                        .inflate(
                                R.layout.automotive_layout_with_horizontal_back_button_toolbar,
                                null);
        mAutomotiveToolbar = parent.findViewById(R.id.back_button_toolbar);
        mAutomotiveBackButtonToolbarCoordinator =
                new AutomotiveBackButtonToolbarCoordinator(
                        activity, mAutomotiveToolbar, mFullscreenManager, mTouchEventProvider);
        mEdgeSwipeGestureDetector =
                mAutomotiveBackButtonToolbarCoordinator.getEdgeSwipeGestureDetectorForTesting();
    }

    @Test
    public void testFullscreen_onEnterFullscreen() {
        Assert.assertEquals(mAutomotiveToolbar.getVisibility(), View.VISIBLE);
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();
        verify(mFullscreenManager).addObserver(mFullscreenObserver);
        mFullscreenObserver.onEnterFullscreen(null, null);
        verify(mTouchEventProvider).addTouchEventObserver(mEdgeSwipeGestureDetector);
        Assert.assertEquals(
                "Toolbar should be gone when entering fullscreen",
                mAutomotiveToolbar.getVisibility(),
                View.GONE);
    }

    @Test
    public void testFullscreen_onExitFullscreen() {
        mAutomotiveToolbar.setVisibility(View.GONE);
        Assert.assertEquals(mAutomotiveToolbar.getVisibility(), View.GONE);
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();
        mFullscreenObserver.onExitFullscreen(null);
        verify(mTouchEventProvider).removeTouchEventObserver(mEdgeSwipeGestureDetector);
        Assert.assertEquals(
                "Toolbar should appear when not in fullscreen",
                mAutomotiveToolbar.getVisibility(),
                View.VISIBLE);
    }

    @Test
    public void testOnSwipe_handleSwipe() {
        Tab tab = Mockito.mock(Tab.class);
        FullscreenOptions fullscreenOptions = Mockito.mock(FullscreenOptions.class);
        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mAutomotiveBackButtonToolbarCoordinator.handleSwipe();

        Assert.assertEquals(
                "Toolbar should be visible on valid swipe",
                mAutomotiveToolbar.getVisibility(),
                View.VISIBLE);
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Toolbar should disappear after 10s",
                mAutomotiveToolbar.getVisibility(),
                View.GONE);
    }
}
