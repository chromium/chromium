// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;

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
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrCardboardTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebXR to check that the device supports both immersive and non-immersive
 * sessions.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages",
    "force-webxr-runtime=cardboard"
})
public class WebXrVrCardboardDeviceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrCardboardTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrCardboardDeviceTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrCardboardTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
    }

    /** Tests that reported WebXR capabilities match expectations. */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrCapabilities() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_capabilities", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepCheckCapabilities()");
        mWebXrVrTestFramework.endTest();
    }
}
