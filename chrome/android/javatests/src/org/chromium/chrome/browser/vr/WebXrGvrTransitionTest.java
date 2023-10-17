// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VR_DON_ENABLED;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;
import org.chromium.chrome.browser.vr.util.GvrTransitionUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** End-to-end tests for transitioning between WebXR's magic window and presentation modes. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages",
    "force-webxr-runtime=gvr"
})
public class WebXrGvrTransitionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            GvrTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrGvrTestFramework mWebXrVrTestFramework;

    public WebXrGvrTransitionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = GvrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrGvrTestFramework(mTestRule);
    }

    /**
     * Tests that WebXR is not exposed if the flag is not on and the page does not have an origin
     * trial token.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"disable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrDisabledWithoutFlagSet() {
        apiDisabledWithoutFlagSetImpl(
                "test_webxr_disabled_without_flag_set", mWebXrVrTestFramework);
    }

    private void apiDisabledWithoutFlagSetImpl(String url, WebXrGvrTestFramework framework) {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.waitOnJavaScriptStep();
        framework.endTest();
    }

    /** Tests that the immersive session promise is rejected if the DON flow is canceled. */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VR_DON_ENABLED})
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationPromiseRejectedIfDonCanceled_WebXr() {
        presentationPromiseRejectedIfDonCanceledImpl(
                "webxr_test_presentation_promise_rejected_if_don_canceled", mWebXrVrTestFramework);
    }

    private void presentationPromiseRejectedIfDonCanceledImpl(
            String url, WebXrGvrTestFramework framework) {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        framework.enterSessionWithUserGesture();
        // Wait until the DON flow appears to be triggered
        // TODO(bsheedy): Make this less hacky if there's ever an explicit way to check if the
        // DON flow is currently active https://crbug.com/758296
        CriteriaHelper.pollUiThread(
                () -> {
                    String currentPackageName = uiDevice.getCurrentPackageName();
                    return currentPackageName != null
                            && currentPackageName.equals("com.google.vr.vrcore");
                },
                "DON flow did not start",
                POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        uiDevice.pressBack();
        framework.waitOnJavaScriptStep();
        framework.endTest();
    }

    /** Tests that the omnibox reappears after exiting an immersive session. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisabledTest(message = "crbug.com/1229236")
    public void testControlsVisibleAfterExitingVr_WebXr() throws InterruptedException {
        controlsVisibleAfterExitingVrImpl("generic_webxr_page", mWebXrVrTestFramework);
    }

    private void controlsVisibleAfterExitingVrImpl(
            String url, final WebXrGvrTestFramework framework) throws InterruptedException {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        GvrTransitionUtils.forceExitVr();
        // The hiding of the controls may only propagate after VR has exited, so give it a chance
        // to propagate. In the worst case this test will erroneously pass, but should never
        // erroneously fail, and should only be flaky if omnibox showing is broken.
        Thread.sleep(100);
        CriteriaHelper.pollUiThread(
                () ->
                        framework
                                        .getRule()
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBrowserControlHiddenRatio()
                                == 0.0,
                "Browser controls did not unhide after exiting VR",
                POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that window.requestAnimationFrame stops firing while in a WebXR immersive session, but
     * resumes afterwards.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWindowRafStopsFiringWhilePresenting_WebXr() throws InterruptedException {
        windowRafStopsFiringWhilePresentingImpl(
                "webxr_test_window_raf_stops_firing_during_immersive_session",
                mWebXrVrTestFramework);
    }

    private void windowRafStopsFiringWhilePresentingImpl(
            String url, WebXrGvrTestFramework framework) throws InterruptedException {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepVerifyBeforePresent()");
        // Pausing of window.rAF is done asynchronously, so wait until that's done.
        final CountDownLatch vsyncPausedLatch = new CountDownLatch(1);
        TestVrShellDelegate.getInstance()
                .setVrShellOnVSyncPausedCallback(
                        () -> {
                            vsyncPausedLatch.countDown();
                        });
        framework.enterSessionWithUserGestureOrFail();
        vsyncPausedLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS);
        framework.executeStepAndWait("stepVerifyDuringPresent()");
        GvrTransitionUtils.forceExitVr();
        framework.executeStepAndWait("stepVerifyAfterPresent()");
        framework.endTest();
    }

    /** Tests that window.rAF continues to fire when we have a non-immersive session. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWindowRafFiresDuringNonImmersiveSession() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_window_raf_fires_during_non_immersive_session", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.waitOnJavaScriptStep();
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that non-immersive sessions stop receiving rAFs during an immersive session, but resume
     * once the immersive session ends.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisabledTest(message = "https://crbug.com/1229236")
    public void testNonImmersiveStopsDuringImmersive() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_non_immersive_stops_during_immersive", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepBeforeImmersive()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("stepDuringImmersive()");
        GvrTransitionUtils.forceExitVr();
        mWebXrVrTestFramework.executeStepAndWait("stepAfterImmersive()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that a permission prompt dismisses by itself when the page navigates away from the
     * current page.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    // TODO(crbug.com/1250492): Re-enable this test on all activity types once
    // WAA/CCT versions no longer fail consistently.
    // @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentDialogIsDismissedWhenPageNavigatesAwayInMainFrame() {
        mWebXrVrTestFramework.setPermissionPromptAction(
                WebXrVrTestFramework.PERMISSION_PROMPT_ACTION_DO_NOTHING);
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGesture();
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "window.location.href = 'https://google.com'", POLL_TIMEOUT_SHORT_MS);
        PermissionUtils.waitForPermissionPromptDismissal();
    }
}
