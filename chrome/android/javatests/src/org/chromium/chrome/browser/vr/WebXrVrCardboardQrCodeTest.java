// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrCardboardTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.webxr.CardboardUtils;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for QR code scanning. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages",
    "force-webxr-runtime=cardboard"
})
public class WebXrVrCardboardQrCodeTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrCardboardTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrCardboardQrCodeTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrCardboardTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        CardboardUtils.useCardboardMockForTesting();
    }

    /**
     * Tests that the QR code scanner is not attempted to be launched when the VR session is entered
     * and there are device parameters saved on the device.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testQrCodeScannerIsNotLaunchedWhenEnteringVrSession() throws Exception {
        mWebXrVrTestFramework.setPermissionPromptAction(
                WebXrVrTestFramework.PERMISSION_PROMPT_ACTION_ALLOW);
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);

        // Ensure that there are saved device parameters.
        CardboardUtils.useCardboardV1DeviceParamsForTesting();

        // Launch VR session.
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        // Assert that MockCardboardSdk::ScanQrCodeAndSaveDeviceParams() has
        // not been executed since there were saved device parameters.
        Assert.assertFalse(CardboardUtils.checkQrCodeScannerWasLaunchedForTesting());
    }

    /**
     * Tests that the QR code scanner is attempted to be launched when the VR session is entered and
     * there are no device parameters saved on the device.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR,Cardboard"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testQrCodeScannerIsLaunchedWhenEnteringVrSession() throws Exception {
        mWebXrVrTestFramework.setPermissionPromptAction(
                WebXrVrTestFramework.PERMISSION_PROMPT_ACTION_ALLOW);
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);
        // Launch VR session.
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        // Assert that MockCardboardSdk::ScanQrCodeAndSaveDeviceParams() has
        // been executed since there were no saved device parameters.
        Assert.assertTrue(CardboardUtils.checkQrCodeScannerWasLaunchedForTesting());
    }
}
