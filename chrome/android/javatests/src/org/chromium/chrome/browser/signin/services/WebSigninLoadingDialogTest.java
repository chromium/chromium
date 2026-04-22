// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.WebSigninRedirectCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.ui.util.RunnableTimer;
import org.chromium.url.GURL;

/** Integration tests for the web signin loading dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(SigninFeatures.ENABLE_WEB_SIGNIN_LOADING_DIALOG)
@Batch(Batch.PER_CLASS)
public class WebSigninLoadingDialogTest {
    private static final long NATIVE_WEB_SIGNIN_BRIDGE = 1000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private WebSigninBridge.Natives mWebSigninBridgeMocks;
    @Mock private RunnableTimer mMockShowDialogTimer;
    @Mock private RunnableTimer mMockMinDialogVisibleTimer;

    @Captor private ArgumentCaptor<Callback<Integer>> mCallbackCaptor;

    @Before
    public void setUp() {
        WebSigninBridgeJni.setInstanceForTesting(mWebSigninBridgeMocks);
        mActivityTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void testShowDialog() {
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.setTabForTesting(mActivityTestRule.getActivityTab());
                    coordinator.showDialog();
                });

        onViewWaiting(withId(R.id.web_signin_loading_dialog)).check(matches(isDisplayed()));
        Assert.assertNotNull(coordinator.getDialogModelForTesting());
    }

    @Test
    @MediumTest
    public void testDialogShownAfterDelay() {
        when(mWebSigninBridgeMocks.createWithEmail(any(), anyString(), any()))
                .thenReturn(NATIVE_WEB_SIGNIN_BRIDGE);
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.initializeWebSigninAndRedirect(
                            mActivityTestRule.getActivityTab(),
                            "test@gmail.com",
                            /* continueUrl */ new GURL("https://continue.url"),
                            /* initialTabURL */ new GURL("about:blank"));
                });

        onViewWaiting(withId(R.id.web_signin_loading_dialog)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCancelButton() {
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.setTabForTesting(mActivityTestRule.getActivityTab());
                    coordinator.showDialog();
                });
        onViewWaiting(withId(R.id.web_signin_loading_dialog)).check(matches(isDisplayed()));
        onView(withId(R.id.cancel_button)).perform(click());

        Assert.assertNull(coordinator.getDialogModelForTesting());
    }

    @Test
    @MediumTest
    public void testDialogNotShownBeforeDelay() {
        when(mWebSigninBridgeMocks.createWithEmail(any(), anyString(), mCallbackCaptor.capture()))
                .thenReturn(NATIVE_WEB_SIGNIN_BRIDGE);
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();
        coordinator.setShowDialogTimerForTesting(mMockShowDialogTimer);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.initializeWebSigninAndRedirect(
                            mActivityTestRule.getActivityTab(),
                            "test@gmail.com",
                            /* continueURL */ new GURL("https://continue.url"),
                            /* initialTabURL */ new GURL("about:blank"));
                    // Result returns before timer fires.
                    mCallbackCaptor.getValue().onResult(WebSigninTrackerResult.SUCCESS);
                });

        Assert.assertNull(coordinator.getDialogModelForTesting());
        verify(mMockShowDialogTimer, atLeastOnce()).cancelTimer();
    }

    @Test
    @MediumTest
    public void testDialogShownForAtLeastMinimumTime() {
        when(mWebSigninBridgeMocks.createWithEmail(any(), anyString(), mCallbackCaptor.capture()))
                .thenReturn(NATIVE_WEB_SIGNIN_BRIDGE);
        WebSigninRedirectCoordinator coordinator = new WebSigninRedirectCoordinator();
        // Use a 0ms delay to show the dialog immediately by running the runnable passed to
        // startTimer.
        coordinator.setShowDialogTimerForTesting(mMockShowDialogTimer);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(1);
                            runnable.run();
                            return null;
                        })
                .when(mMockShowDialogTimer)
                .startTimer(anyLong(), any(Runnable.class));

        coordinator.setMinDialogVisibleTimerForTesting(mMockMinDialogVisibleTimer);
        final ArgumentCaptor<Runnable> minShowTimeRunnableCaptor =
                ArgumentCaptor.forClass(Runnable.class);
        doAnswer(
                        invocation -> {
                            // Do nothing immediately, just capture the runnable.
                            return null;
                        })
                .when(mMockMinDialogVisibleTimer)
                .startTimer(anyLong(), minShowTimeRunnableCaptor.capture());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.initializeWebSigninAndRedirect(
                            mActivityTestRule.getActivityTab(),
                            "test@gmail.com",
                            /* continueUrl */ new GURL("https://continue.url"),
                            /* initialTabURL */ new GURL("about:blank"));
                });

        onViewWaiting(withId(R.id.web_signin_loading_dialog)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Result returns but mock timer has not "fired" yet.
                    mCallbackCaptor.getValue().onResult(WebSigninTrackerResult.SUCCESS);
                });

        // Dialog should still be shown.
        onView(withId(R.id.web_signin_loading_dialog)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Fire the timer.
                    minShowTimeRunnableCaptor.getValue().run();
                });

        // Dialog should be dismissed.
        Assert.assertNull(coordinator.getDialogModelForTesting());
    }
}
