// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ArPlaybackFile;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for testing WebXR for AR's anchors behavior. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXR,WebXRARModule,WebXRHitTest,LogJsConsoleMessages"
})
public class WebXrArAnchorsTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArAnchorsTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /** Tests that anchor can be created off of a valid hit test result. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testHitTestAnchorSucceedsWithPlane() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_anchors_hittest", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting()");
        mWebXrArTestFramework.endTest();
    }

    /** Tests that a free-floating anchor can be created when the session is stable. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testFreeFloatingAnchorSucceeds() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_anchors_freefloating", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepStartTest()");
        mWebXrArTestFramework.endTest();
    }

    /**
     * Tests that an anchor gets updated (includes updating anchor position, tracking pause, and
     * tracking recovery).
     */
    @Test
    @LargeTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile(
            "chrome/test/data/xr/ar_playback_datasets/floor_session_with_tracking_loss_37s_30fps.mp4")
    public void testAnchorStates() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_anchors_updates", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        // The recording is 37 seconds long, let's wait for a bit more than that before timing out.
        mWebXrArTestFramework.executeStepAndWait("stepStartHitTesting()", 40 * 1000);
        mWebXrArTestFramework.endTest();
        // Time taken from start to end of JS test should not be less than 20 seconds (tracking loss
        // happens later in the recording).
        mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                "time_taken_in_ms > (20 * 1000)", POLL_TIMEOUT_SHORT_MS);
    }
}
