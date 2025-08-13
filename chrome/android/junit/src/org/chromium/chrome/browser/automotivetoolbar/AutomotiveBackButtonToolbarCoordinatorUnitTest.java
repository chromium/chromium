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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.display.DisplayUtil;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class AutomotiveBackButtonToolbarCoordinatorUnitTest {
    private static final int ANIMATION_DURATION_MS = 400;
    private static final int ON_SWIPE_TOOLBAR_DURATION_MS = 10000;

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
        DisplayUtil.setCarmaPhase1Version2ComplianceForTesting(true);
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
    @EnableFeatures({ChromeFeatureList.AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE})
    public void testAutomotiveBackButtonBarStreamline_onEnterAndExitFullScreen() {
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();

        verify(mFullscreenManager).addObserver(mFullscreenObserver);

        mFullscreenObserver.onEnterFullscreen(null, null);

        Assert.assertEquals(
                "Back button toolbar should be gone when entering full screen.",
                View.GONE,
                mAutomotiveToolbar.getVisibility());

        // Check the that toolbar is visible when the user swipes in full screen.
        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();

        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should be visible on valid swipe.",
                View.VISIBLE,
                mOnSwipeAutomotiveToolbar.getVisibility());
        ShadowLooper.idleMainLooper(ON_SWIPE_TOOLBAR_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should disappear after 10s.",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());

        mFullscreenObserver.onExitFullscreen(null);
        Assert.assertEquals(
                "Back button toolbar should be gone when exiting full screen.",
                View.GONE,
                mAutomotiveToolbar.getVisibility());
    }

    @Test
    public void testFullscreen_onEnterFullscreen() {
        Assert.assertEquals(View.VISIBLE, mAutomotiveToolbar.getVisibility());
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();

        verify(mFullscreenManager).addObserver(mFullscreenObserver);
        mFullscreenObserver.onEnterFullscreen(null, null);
        Assert.assertEquals(
                "Toolbar should be gone when entering fullscreen",
                View.GONE,
                mAutomotiveToolbar.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE)
    public void testFullscreen_onExitFullscreen() {
        mFullscreenObserver =
                mAutomotiveBackButtonToolbarCoordinator.getFullscreenObserverForTesting();
        mFullscreenObserver.onEnterFullscreen(null, null);
        Assert.assertEquals(View.GONE, mAutomotiveToolbar.getVisibility());
        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
        Assert.assertEquals(View.VISIBLE, mOnSwipeAutomotiveToolbar.getVisibility());
        mFullscreenObserver.onExitFullscreen(null);

        verify(mTouchEventProvider).removeTouchEventObserver(mEdgeSwipeGestureDetector);
        Assert.assertEquals(
                "On swipe toolbar should not be gone when not in fullscreen",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());
        Assert.assertEquals(
                "Toolbar should appear when not in fullscreen",
                View.VISIBLE,
                mAutomotiveToolbar.getVisibility());
    }

    @Test
    public void testOnSwipe_handleSwipe() {
        Tab tab = Mockito.mock(Tab.class);
        FullscreenOptions fullscreenOptions = Mockito.mock(FullscreenOptions.class);
        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();

        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should be visible on valid swipe",
                View.VISIBLE,
                mOnSwipeAutomotiveToolbar.getVisibility());
        ShadowLooper.idleMainLooper(ON_SWIPE_TOOLBAR_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should disappear after 10s",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());
    }

    @Test
    public void testOnBackSwipe_handleBackSwipe() {
        Tab tab = Mockito.mock(Tab.class);
        FullscreenOptions fullscreenOptions = Mockito.mock(FullscreenOptions.class);
        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "On swipe toolbar should be gone after a back swipe",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());
    }

    @Test
    public void testMultipleSwipes_handleForwardSwipe() {
        Tab tab = Mockito.mock(Tab.class);
        FullscreenOptions fullscreenOptions = Mockito.mock(FullscreenOptions.class);
        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "First swipe gesture will be consumed to show the toolbar, and the backswipe will"
                        + " be ignored",
                View.VISIBLE,
                mOnSwipeAutomotiveToolbar.getVisibility());

        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();
        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "First swipe gesture will be consumed to show the toolbar, and the backswipe and"
                        + " following forward swipe will be ignored",
                View.VISIBLE,
                mOnSwipeAutomotiveToolbar.getVisibility());
    }

    @Test
    public void testMultipleSwipes_handleBackSwipe() {
        Tab tab = Mockito.mock(Tab.class);
        FullscreenOptions fullscreenOptions = Mockito.mock(FullscreenOptions.class);
        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();
        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "First swipe gesture will be consumed to hide the toolbar, and the forward swipe"
                        + " will be ignored",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());

        mAutomotiveBackButtonToolbarCoordinator
                .getFullscreenObserverForTesting()
                .onEnterFullscreen(tab, fullscreenOptions);
        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleSwipe();
        mAutomotiveBackButtonToolbarCoordinator.getOnSwipeCallbackForTesting().handleBackSwipe();
        ShadowLooper.idleMainLooper(ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "First swipe gesture will be consumed to hide the toolbar, and the foward swipe and"
                        + " following back swipe will be ignored",
                View.GONE,
                mOnSwipeAutomotiveToolbar.getVisibility());
    }
}
