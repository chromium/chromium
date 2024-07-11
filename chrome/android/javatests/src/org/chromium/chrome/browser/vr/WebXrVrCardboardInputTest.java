// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrCardboardTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.webxr.CardboardUtils;
import org.chromium.components.webxr.XrSessionCoordinator;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for sending input while using WebXR. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages",
    "force-webxr-runtime=cardboard"
})
public class WebXrVrCardboardInputTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrCardboardTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrCardboardInputTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrCardboardTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        CardboardUtils.useCardboardV1DeviceParamsForTesting();
    }

    private long sendScreenTouchDownToView(final View view, final int x, final int y) {
        long downTime = SystemClock.uptimeMillis();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.dispatchTouchEvent(
                            MotionEvent.obtain(
                                    downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0));
                });
        return downTime;
    }

    private void sendScreenTouchUpToView(
            final View view, final int x, final int y, final long downTime) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    long now = SystemClock.uptimeMillis();
                    view.dispatchTouchEvent(
                            MotionEvent.obtain(downTime, now, MotionEvent.ACTION_UP, x, y, 0));
                });
    }

    private void sendScreenTapToView(final View view, final int x, final int y) {
        long downTime = sendScreenTouchDownToView(view, x, y);
        SystemClock.sleep(100);
        sendScreenTouchUpToView(view, x, y, downTime);
        SystemClock.sleep(100);
    }

    private void spamScreenTapsToXrSession(
            final XrSessionCoordinator xrSession, final int x, final int y, final int iterations) {
        // Tap the screen a bunch of times.
        // Android doesn't seem to like sending touch events too quickly, so have a short delay
        // between events.
        for (int i = 0; i < iterations; i++) {
            sendScreenTapToXrSession(xrSession, x, y);
        }
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

    /**
     * Tests that screen touches are registered as XR input in immersive sessions, when the viewer
     * is Cardboard.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testScreenTapsRegistered_WebXr() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_input", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        int numIterations = 5;
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "stepSetupListeners(" + String.valueOf(numIterations) + ")", POLL_TIMEOUT_SHORT_MS);

        XrSessionCoordinator activeSession = XrSessionCoordinator.getActiveInstanceForTesting();
        int x = mWebXrVrTestFramework.getCurrentContentView().getWidth() / 2;
        int y = mWebXrVrTestFramework.getCurrentContentView().getHeight() / 2;

        for (int i = 0; i < numIterations; i++) {
            sendScreenTapToXrSession(activeSession, x, y);
            mWebXrVrTestFramework.waitOnJavaScriptStep();
        }

        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that screen touches are registered as transient XR input in inline sessions, when the
     * viewer is Cardboard.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testTransientScreenTapsRegistered_WebXr() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_transient_input", PAGE_LOAD_TIMEOUT_S);

        int numIterations = 10;
        View presentationView = mWebXrVrTestFramework.getCurrentContentView();
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "stepSetupListeners(" + String.valueOf(numIterations) + ")", POLL_TIMEOUT_SHORT_MS);

        int x = presentationView.getWidth() / 2;
        int y = presentationView.getHeight() / 2;

        // Tap the screen a bunch of times and make sure that they're all registered. Ideally, we
        // shouldn't have to ack each one, but it's possible for inputs to get eaten by garbage
        // collection if there are multiple in flight, so only send one at a time.
        for (int i = 0; i < numIterations; i++) {
            sendScreenTapToView(presentationView, x, y);
            mWebXrVrTestFramework.waitOnJavaScriptStep();
        }

        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that focus is locked to the device with an immersive session for the purposes of VR
     * input.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationLocksFocus_WebXr() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_presentation_locks_focus", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("stepSetupFocusLoss()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Verifies that the XRSession has an input source when using WebXR and Cardboard. There should
     * be no gamepads on the input source or navigator array.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrInputSourceWithoutGamepad() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_gamepad_support", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        // Spam input to make sure the Gamepad API registers the gamepad if it should.
        int numIterations = 10;
        int x = mWebXrVrTestFramework.getCurrentContentView().getWidth() / 2;
        int y = mWebXrVrTestFramework.getCurrentContentView().getHeight() / 2;

        spamScreenTapsToXrSession(
                XrSessionCoordinator.getActiveInstanceForTesting(), x, y, numIterations);

        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "inputSourceHasNoGamepad()", POLL_TIMEOUT_SHORT_MS);

        mWebXrVrTestFramework.runJavaScriptOrFail("done()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.endTest();
    }
}
