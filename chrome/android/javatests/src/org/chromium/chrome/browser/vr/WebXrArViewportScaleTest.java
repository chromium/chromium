// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;

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

/** End-to-end tests for testing WebXR for dynamic viewport scale behavior in AR mode. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXR,WebXRARModule,WebXRHitTest,LogJsConsoleMessages"
})
public class WebXrArViewportScaleTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArViewportScaleTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /** Tests that viewport scaling works when requested at the start of a frame. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile(
            "chrome/test/data/xr/ar_playback_datasets/floor_session_with_tracking_loss_37s_30fps.mp4")
    public void testViewportScaleSameFrame() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_viewport_scale", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepRequestViewportScaleSameFrame()");
        mWebXrArTestFramework.endTest();
    }

    /** Tests that viewport scaling applies to the next frame if requested after gl.viewport. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile(
            "chrome/test/data/xr/ar_playback_datasets/floor_session_with_tracking_loss_37s_30fps.mp4")
    public void testViewportScaleNextFrame() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_viewport_scale", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepRequestViewportScaleNextFrame()");
        mWebXrArTestFramework.endTest();
    }

    /** Tests that dynamic viewport scaling using the recommended viewport scale works. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile(
            "chrome/test/data/xr/ar_playback_datasets/floor_session_with_tracking_loss_37s_30fps.mp4")
    public void testRecommendedViewportScale() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_viewport_scale", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.executeStepAndWait("stepRequestRecommendedViewportScale()");
        mWebXrArTestFramework.endTest();
    }
}
