// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.MotionEventTestUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class GestureUserEducationIphControllerUnitTest {
    private static final int PAGE_LOAD_DELAY = 4000;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BackPressManager mBackPressManager;
    @Mock private ScrimManager mScrimManager;
    private ViewGroup mAnchorView;
    @Mock private Tab mTab;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private WebContentsAccessibility mWebContentsAccessibility;
    @Mock private NavigationController mNavigationController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Window mWindow;
    private Activity mActivity;

    private GestureUserEducationIphController mController;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mAnchorView = new FrameLayout(mActivity);
                        });
        mController =
                new GestureUserEducationIphController(
                        mAnchorView, mActivityTabProvider, mBackPressManager, mScrimManager);
        mController.setWebContentsAccessibilityForTesting(mWebContentsAccessibility);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getContext()).thenReturn(mActivity);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getWindow()).thenReturn(mWindow);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
    }

    @Test
    public void testShowIph() {
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
        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);
        verify(mScrimManager).showScrim(any());
        Assert.assertEquals(1, mAnchorView.getChildCount());

        verify(mWebContentsAccessibility).setObscuredByAnotherView(true);
    }

    @Test
    public void testNoShowIph_NoBackHistory() {
        mController.setIsGestureNavModeForTesting(true);
        when(mNavigationController.canGoToOffset(
                        GestureUserEducationIphController.PAGE_HISTORY_MIN_OFFSET))
                .thenReturn(false);
        when(mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION))
                .thenReturn(true);

        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mScrimManager, never()).showScrim(any());
        Assert.assertEquals(0, mAnchorView.getChildCount());
    }

    @Test
    public void testNoShowIph_BackPressNotHandled() {
        mController.setIsGestureNavModeForTesting(true);
        when(mNavigationController.canGoToOffset(
                        GestureUserEducationIphController.PAGE_HISTORY_MIN_OFFSET))
                .thenReturn(true);
        when(mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY))
                .thenReturn(false);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION))
                .thenReturn(true);

        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mScrimManager, never()).showScrim(any());
        Assert.assertEquals(0, mAnchorView.getChildCount());
    }

    @Test
    public void testNoShowIph_TrackerSaysNo() {
        mController.setIsGestureNavModeForTesting(true);
        when(mNavigationController.canGoToOffset(
                        GestureUserEducationIphController.PAGE_HISTORY_MIN_OFFSET))
                .thenReturn(true);
        when(mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION))
                .thenReturn(false);

        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mScrimManager, never()).showScrim(any());
        Assert.assertEquals(0, mAnchorView.getChildCount());
    }

    @Test
    public void testHideIph() {
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
        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);

        ArgumentCaptor<PropertyModel> scrimPropertyModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mScrimManager).showScrim(scrimPropertyModelCaptor.capture());
        Assert.assertEquals(
                "Layout should be present before hiding", 1, mAnchorView.getChildCount());

        scrimPropertyModelCaptor
                .getValue()
                .get(ScrimProperties.GESTURE_DETECTOR)
                .onTouchEvent(MotionEventTestUtils.getTrackpadTouchDownEventNoClick());
        ShadowLooper.idleMainLooper();

        verify(mScrimManager).hideScrim(any(), anyBoolean());
        Assert.assertEquals(0, mAnchorView.getChildCount());

        verify(mWebContentsAccessibility).setObscuredByAnotherView(false);
    }

    @Test
    public void testOnObservingDifferentTab_HidesIph() {
        mController.setIsGestureNavModeForTesting(true);
        when(mNavigationController.canGoToOffset(
                        GestureUserEducationIphController.PAGE_HISTORY_MIN_OFFSET))
                .thenReturn(true);
        when(mBackPressManager.isBackPressHandlerConsumingBackEvent(
                        BackPressHandler.Type.TAB_HISTORY))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.GESTURE_USER_EDUCATION))
                .thenReturn(true);

        // Set the tab to something non-null first.
        mActivityTabProvider.setForTesting(mTab);

        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);

        Assert.assertTrue(mAnchorView.getChildCount() > 0);

        // Switch to a different tab (null in this case).
        mActivityTabProvider.setForTesting(null);

        verify(mScrimManager).hideScrim(any(), anyBoolean());
        Assert.assertEquals(0, mAnchorView.getChildCount());
        verify(mWebContentsAccessibility).setObscuredByAnotherView(false);
    }

    @Test
    public void testOnPageLoadStarted_HidesIph() {
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
        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);

        Assert.assertEquals(1, mAnchorView.getChildCount());

        tabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mScrimManager).hideScrim(any(), anyBoolean());
        Assert.assertEquals(0, mAnchorView.getChildCount());
        verify(mWebContentsAccessibility).setObscuredByAnotherView(false);
    }

    @Test
    public void testOnPageLoadStarted_CancelsScheduledIph() {
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
        tabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.EXAMPLE_URL);
        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);

        verify(mScrimManager, never()).showScrim(any());
        Assert.assertEquals(0, mAnchorView.getChildCount());
    }

    @Test
    public void testOnObservingDifferentTab_CancelsScheduledIph() {
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

        // Set the tab to something non-null first so that switching to null triggers
        // onObservingDifferentTab.
        mActivityTabProvider.setForTesting(mTab);

        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        // Switch to a different tab (null in this case) triggers onObservingDifferentTab.
        mActivityTabProvider.setForTesting(null);

        ShadowLooper.idleMainLooper(PAGE_LOAD_DELAY, TimeUnit.MILLISECONDS);

        verify(mScrimManager, never()).showScrim(any());
        Assert.assertEquals(0, mAnchorView.getChildCount());
    }
}
