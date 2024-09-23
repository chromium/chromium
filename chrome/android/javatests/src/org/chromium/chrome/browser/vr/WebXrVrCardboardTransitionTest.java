// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrCardboardTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.webxr.CardboardUtils;
import org.chromium.components.webxr.XrSessionCoordinator;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for transitioning between WebXR's magic window and presentation modes. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages",
    "force-webxr-runtime=cardboard"
})
public class WebXrVrCardboardTransitionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrCardboardTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrCardboardTransitionTest(Callable<ChromeActivityTestRule> callable)
            throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrCardboardTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        CardboardUtils.useCardboardV1DeviceParamsForTesting();
    }

    /** Tests that WebXR is not exposed if the flag is not on. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"disable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrDisabledWithoutFlagSet() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_disabled_without_flag_set", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.waitOnJavaScriptStep();
        mWebXrVrTestFramework.endTest();
    }

    /** Tests that the omnibox reappears after exiting an immersive session. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testControlsVisibleAfterExitingVr_WebXr() throws InterruptedException {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        forceExitVr();
        // The hiding of the controls may only propagate after VR has exited, so give it a chance
        // to propagate. In the worst case this test will erroneously pass, but should never
        // erroneously fail, and should only be flaky if omnibox showing is broken.
        Thread.sleep(100);
        CriteriaHelper.pollUiThread(
                () ->
                        mWebXrVrTestFramework
                                        .getRule()
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBrowserControlHiddenRatio()
                                == 0.0,
                "Browser controls did not unhide after exiting VR",
                POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        mWebXrVrTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that window.requestAnimationFrame stops firing while in a WebXR immersive session, but
     * resumes afterwards.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWindowRafStopsFiringWhilePresenting_WebXr() throws InterruptedException {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_window_raf_stops_firing_during_immersive_session", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepVerifyBeforePresent()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("stepVerifyDuringPresent()");
        forceExitVr();
        mWebXrVrTestFramework.executeStepAndWait("stepVerifyAfterPresent()");
        mWebXrVrTestFramework.endTest();
    }

    /** Tests that window.rAF continues to fire when we have a non-immersive session. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
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
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisabledTest(message = "https://crbug.com/1229236")
    public void testNonImmersiveStopsDuringImmersive() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_non_immersive_stops_during_immersive", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepBeforeImmersive()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("stepDuringImmersive()");
        forceExitVr();
        mWebXrVrTestFramework.executeStepAndWait("stepAfterImmersive()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that a permission prompt dismisses by itself when the page navigates away from the
     * current page.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    // TODO(crbug.com/40791908): Re-enable this test on all activity types once
    // WAA/CCT versions no longer fail consistently.
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentDialogIsDismissedWhenPageNavigatesAwayInMainFrame() {
        mWebXrVrTestFramework.setPermissionPromptAction(
                WebXrVrTestFramework.PERMISSION_PROMPT_ACTION_DO_NOTHING);
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGesture();
        String genericPagePath =
                mWebXrVrTestFramework.getUrlForFile("generic_webxr_permission_page");
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "window.location.href = '" + genericPagePath + "'", POLL_TIMEOUT_SHORT_MS);
        PermissionUtils.waitForPermissionPromptDismissal();
    }

    /** Forces Chrome out of VR mode. */
    private static void forceExitVr() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    XrSessionCoordinator.endActiveSession();
                });
    }
}
