// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.graphics.PointF;
import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.NfcSimUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebXR where the choice of test device has a greater
 * impact than the usual Daydream-ready vs. non-Daydream-ready effect.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
public class WebXrVrDeviceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrDeviceTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
    }

    /**
     * Tests that reported WebXR capabilities match expectations.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
            public void testWebXrCapabilities() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_capabilities"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepCheckCapabilities('Daydream')");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter immersive mode by clicking on 'Enter VR' button displayed on a VR content page
     *   * Check for non-null poses
     *   * Enter inline VR mode by clicking the 'app' button on the controller
     *   * Check for null poses
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
            @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
            public void testForNullPosesInInlineVrPostImmersive() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterMagicWindowSessionWithUserGestureOrFail();

        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
        mWebXrVrTestFramework.pollJavaScriptBoolean(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
                POLL_TIMEOUT_SHORT_MS);

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter inline VR mode using NFC scan while browser points at a VR content page
     *   * Check for null poses
     *   * Enter immersive mode by clicking the 'Enter VR' button on the page
     *   * Check for non-null poses
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
    public void testForNullPosesInInlineVrFromNfc() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);
        NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());

        mWebXrVrTestFramework.enterMagicWindowSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter inline VR mode using NFC scan while browser points at a blank page
     *   * Point the browser at a page with VR content
     *   * Check for null poses
     *   * Enter immersive mode by clicking the 'Enter VR' button on the page
     *   * Check for non-null poses
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
    public void testForNullPosesInInlineVrOnNavigation() {
        NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);

        mWebXrVrTestFramework.enterMagicWindowSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }
}
