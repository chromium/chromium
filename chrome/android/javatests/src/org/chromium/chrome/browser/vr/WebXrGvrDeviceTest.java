// Copyright 2017 The Chromium Authors
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
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;
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
public class WebXrGvrDeviceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            GvrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrGvrTestFramework mWebXrVrTestFramework;

    public WebXrGvrDeviceTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = GvrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrGvrTestFramework(mTestRule);
    }

    /**
     * Tests that reported WebXR capabilities match expectations.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrCapabilities() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_capabilities", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepCheckCapabilities()");
        mWebXrVrTestFramework.endTest();
    }
}
