// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.WebXrArTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.WebXrArTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.SystemClock;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ArPlaybackFile;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.webxr.XrSessionCoordinator;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for testing WebXR for AR's hit testing behavior. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXR,WebXRARModule,WebXRHitTest,LogJsConsoleMessages"
})
public class WebXrArHitTestTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private final ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArHitTestTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /** Tests that hit test returns a valid result when there is a plane present. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestSucceedsWithPlane() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_hittest", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting()");
        mWebXrArTestFramework.endTest();
    }

    /**
     * Tests that hit test results are available in the subsequent frame after hit test source was
     * returned.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestResultsAvailableInSubsequentFrame() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_hittest_results_availability", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting()");
        mWebXrArTestFramework.endTest();
    }

    /** Tests that hit test cancellation works for hit test sources when the session has ended. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestCancellationWorks() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_hittest_cancellation", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting(false)");
        mWebXrArTestFramework.endTest();
    }

    /**
     * Tests that hit test cancellation works for transient input hit tests when the session has
     * ended.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestForTransientInputCancellationWorks() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_hittest_cancellation", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting(true)");
        mWebXrArTestFramework.endTest();
    }

    private void sendScreenTapToXrSession(
            final XrSessionCoordinator xrSession, final int x, final int y) {
        sendScreenTouchDownToXrSession(xrSession, x, y);
        SystemClock.sleep(100);
        sendScreenTouchUpToXrSession(xrSession, x, y);
        SystemClock.sleep(100);
    }

    private void sendScreenTouchDownToXrSession(
            final XrSessionCoordinator xrSession, final int x, final int y) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    xrSession.onDrawingSurfaceTouch(true, true, 0, x, y);
                });
    }

    private void sendScreenTouchUpToXrSession(
            final XrSessionCoordinator xrSession, final int x, final int y) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    xrSession.onDrawingSurfaceTouch(true, false, 0, x, y);
                });
    }

    /** Tests that hit test returns a valid result in a click event. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestForTransientInputValidInClickEvent() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_transient_hit_test_click", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.runJavaScriptOrFail("stepStartHitTesting()", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                "testState == TestState.HitTestSourceAvailable", POLL_TIMEOUT_LONG_MS);

        int retries = 10;
        int x = mWebXrArTestFramework.getCurrentContentView().getWidth() / 2;
        int y = mWebXrArTestFramework.getCurrentContentView().getHeight() / 2;
        XrSessionCoordinator coordinator = XrSessionCoordinator.getActiveInstanceForTesting();
        boolean testDone = false;
        while (!testDone && retries > 0) {
            sendScreenTapToXrSession(coordinator, x, y);
            testDone =
                    mWebXrArTestFramework.pollJavaScriptBoolean(
                            "testState == TestState.Done", POLL_TIMEOUT_SHORT_MS);
            retries--;
        }
        Assert.assertTrue(testDone);
        mWebXrArTestFramework.endTest();
    }
}
