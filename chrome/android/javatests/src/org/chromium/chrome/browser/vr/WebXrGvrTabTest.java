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
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.WebContents;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebXR's behavior when multiple tabs are involved.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
public class WebXrGvrTabTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            GvrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrGvrTestFramework mWebXrVrTestFramework;

    public WebXrGvrTabTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = GvrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrGvrTestFramework(mTestRule);
    }

    /**
     * Tests that non-focused tabs don't get WebXR rAFs called. Disabled on standalones because
     * they will always be in the VR Browser, and thus shouldn't be getting inline poses even
     * if the tab is focused.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    public void testPoseDataUnfocusedTab_WebXr() {
        testPoseDataUnfocusedTabImpl("webxr_test_pose_data_unfocused_tab", mWebXrVrTestFramework);
    }

    private void testPoseDataUnfocusedTabImpl(String url, WebXrGvrTestFramework framework) {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepCheckFrameDataWhileFocusedTab()");
        WebContents firstTabContents = framework.getCurrentWebContents();

        mTestRule.loadUrlInNewTab("about:blank");

        WebXrGvrTestFramework.executeStepAndWait(
                "stepCheckFrameDataWhileNonFocusedTab()", firstTabContents);
        WebXrGvrTestFramework.endTest(firstTabContents);
    }
}
