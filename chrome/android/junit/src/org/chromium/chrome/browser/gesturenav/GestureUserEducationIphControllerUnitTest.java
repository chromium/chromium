// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;
import android.view.Window;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
public class GestureUserEducationIphControllerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BackPressManager mBackPressManager;
    @Mock private ScrimManager mScrimManager;
    @Mock private ViewStub mViewStub;
    @Mock private View mIphContainer;
    @Mock private Tab mTab;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Window mWindow;
    private Activity mActivity;

    private GestureUserEducationIphController mController;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mViewStub.inflate()).thenReturn(mIphContainer);
        mController =
                new GestureUserEducationIphController(
                        mViewStub, mActivityTabProvider, mBackPressManager, mScrimManager);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getWindow()).thenReturn(mWindow);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
    }

    @Test
    public void showIph() {
        mController.setIsGestureNavModeForTesting(true);
        when(mNavigationController.canGoToOffset(
                        GestureUserEducationIphController.PAGE_HISTORY_MIN_OFFSET))
                .thenReturn(true);
        when(mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION))
                .thenReturn(true);

        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mScrimManager).showScrim(any());
    }

    // TODO(crbug.com/493307156): Add more tests after implementing IPH with scrim and animation.
}
