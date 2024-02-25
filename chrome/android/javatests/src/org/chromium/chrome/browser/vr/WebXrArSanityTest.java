// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.Build;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ArPlaybackFile;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end test that enables all AR-related features and ensures that the session is stable. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXRIncubations,LogJsConsoleMessages"
})
public class WebXrArSanityTest {
    public static final boolean ENABLE_CAMERA_ACCESS =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArSanityTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /**
     * Tests that a session is functional with all AR-related features enabled - short recording.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    @DisabledTest(message = "https://crbug.com/1515317")
    public void testShortRecording() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_all_ar_features", PAGE_LOAD_TIMEOUT_S);

        if (!ENABLE_CAMERA_ACCESS) {
            mWebXrArTestFramework.runJavaScriptOrFail(
                    "disableCameraAccess()", POLL_TIMEOUT_SHORT_MS);
        }

        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(
                /* needsCameraPermission= */ ENABLE_CAMERA_ACCESS);

        // The recording is 12 seconds long, let's tell the test to run for 10 seconds and wait for
        // a bit more than that before timing out.
        mWebXrArTestFramework.executeStepAndWait("stepStartTest(10)", 15 * 1000);
        mWebXrArTestFramework.endTest();
    }

    /** Tests that a session is functional with all AR-related features enabled - long recording. */
    @Test
    @LargeTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @ArPlaybackFile(
            "chrome/test/data/xr/ar_playback_datasets/floor_session_with_tracking_loss_37s_30fps.mp4")
    @DisabledTest(message = "https://crbug.com/1502764")
    public void testLongRecording() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_basic_all_ar_features", PAGE_LOAD_TIMEOUT_S);

        if (!ENABLE_CAMERA_ACCESS) {
            mWebXrArTestFramework.runJavaScriptOrFail(
                    "disableCameraAccess()", POLL_TIMEOUT_SHORT_MS);
        }

        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(
                /* needsCameraPermission= */ ENABLE_CAMERA_ACCESS);

        // The recording is 37 seconds long, let's tell the test to run for 30 seconds and wait for
        // a bit more than that before timing out.
        mWebXrArTestFramework.executeStepAndWait("stepStartTest(30)", 40 * 1000);
        mWebXrArTestFramework.endTest();
    }
}
