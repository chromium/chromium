// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import static org.mockito.Mockito.verify;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

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
import org.chromium.chrome.browser.back_press.BackPressManager;
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
    private View mOnSwipeAutomotiveToolbar;
    private FullscreenManager.Observer mFullscreenObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private FullscreenManager mFullscreenManager;
    @Mock private BackPressManager mBackPressManager;
    @Mock private TouchEventProvider mTouchEventProvider;
    @Mock private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        FrameLayout parent =
                (FrameLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout
                                                .automotive_layout_with_vertical_back_button_toolbar,
                                        null);
        mAutomotiveToolbar = parent.findViewById(R.id.back_button_toolbar);
        mAutomotiveBackButtonToolbarCoordinator =
                new AutomotiveBackButtonToolbarCoordinator(
                        activity,
                        parent,
                        mFullscreenManager,
                        mTouchEventProvider,
                        mBackPressManager);
        mEdgeSwipeGestureDetector =
                mAutomotiveBackButtonToolbarCoordinator.getEdgeSwipeGestureDetectorForTesting();
        mOnSwipeAutomotiveToolbar =
                parent.findViewById(R.id.automotive_on_swipe_back_button_toolbar);
        mAutomotiveToolbar = parent.findViewById(R.id.back_button_toolbar);
    }

    @Test
    public void testFullscreen_onEnterFullscreen() {
        Assert.assertEquals(mAutomotiveToolbar.getVisibility(), View.VISIBLE);
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();

        verify(mFullscreenManager).addObserver(mFullscreenObserver);
        mFullscreenObserver.onEnterFullscreen(null, null);
        Assert.assertEquals(
                "Toolbar should be gone when entering fullscreen",
                mAutomotiveToolbar.getVisibility(),
                View.GONE);
    }

    @Test
    public void testFullscreen_onExitFullscreen() {
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();
        mFullscreenObserver.onEnterFullscreen(null, null);
        Assert.assertEquals(mAutomotiveToolbar.getVisibility(), View.GONE);
        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
        Assert.assertEquals(mOnSwipeAutomotiveToolbar.getVisibility(), View.VISIBLE);
        mFullscreenObserver.onExitFullscreen(null);

        verify(mTouchEventProvider).removeTouchEventObserver(mEdgeSwipeGestureDetector);
        Assert.assertEquals(
                "On swipe toolbar should not be gone when not in fullscreen",
                mOnSwipeAutomotiveToolbar.getVisibility(),
                View.GONE);
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
                "On swipe toolbar should be visible on valid swipe",
                mOnSwipeAutomotiveToolbar.getVisibility(),
                View.VISIBLE);
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should disappear after 10s",
                mOnSwipeAutomotiveToolbar.getVisibility(),
                View.GONE);
    }
}
