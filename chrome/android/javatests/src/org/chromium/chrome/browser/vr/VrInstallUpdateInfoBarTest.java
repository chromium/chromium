// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;

import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.mock.MockVrCoreVersionChecker;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrInfoBarUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for the InfoBar that prompts the user to update or install
 * VrCore (VR Services) when attempting to use a VR feature with an outdated
 * or entirely missing version or other VR-related update prompts.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction(RESTRICTION_TYPE_SVR)
public class VrInstallUpdateInfoBarTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;

    public VrInstallUpdateInfoBarTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mVrTestRule);
    }

    /**
     * Creates and sets a MockVrCoreVersionCheckerImpl as the VrShellDelegate's VrCoreVersionChecker
     * instance.
     *
     * @param compatibility An int corresponding to a VrCoreCompatibility value that the mock
     *        version checker will return.
     * @return The MockVrCoreVersionCheckerImpl that was set as VrShellDelegate's
     *        VrCoreVersionChecker instance.
     */
    private static MockVrCoreVersionChecker setVrCoreCompatibility(int compatibility) {
        final MockVrCoreVersionChecker mockChecker = new MockVrCoreVersionChecker();
        mockChecker.setMockReturnValue(compatibility);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { VrCoreInstallUtils.overrideVrCoreVersionChecker(mockChecker); });
        Assert.assertEquals("Overriding VrCoreVersionChecker failed", compatibility,
                mockChecker.getLastReturnValue());
        return mockChecker;
    }

    /**
     * Helper function to run the tests checking for the upgrade/install InfoBar being present since
     * all that differs is the value returned by VrCoreVersionChecker and a couple asserts.
     *
     * @param checkerReturnCompatibility The compatibility to have the VrCoreVersionChecker return.
     */
    private void infoBarTestHelper(final int checkerReturnCompatibility) {
        VrCoreInstallUtils vrCoreInstallUtils = VrCoreInstallUtils.create(0);
        setVrCoreCompatibility(checkerReturnCompatibility);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            vrCoreInstallUtils.requestInstallVrCore(
                    mVrTestRule.getActivity().getCurrentWebContents());
        });
        View decorView = mVrTestRule.getActivity().getWindow().getDecorView();
        if (checkerReturnCompatibility == VrCoreVersionChecker.VrCoreCompatibility.VR_READY) {
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, false);
        } else if (checkerReturnCompatibility
                        == VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE
                || checkerReturnCompatibility
                        == VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_AVAILABLE) {
            // Out of date and missing cases are the same, but with different text
            String expectedMessage;
            String expectedButton;
            if (checkerReturnCompatibility
                    == VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE) {
                expectedMessage = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_update_text);
                expectedButton = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_update_button);
            } else {
                expectedMessage = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_install_text);
                expectedButton = ContextUtils.getApplicationContext().getString(
                        org.chromium.chrome.vr.R.string.vr_services_check_infobar_install_button);
            }
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, true);
            TextView tempView = (TextView) decorView.findViewById(R.id.infobar_message);
            Assert.assertEquals("VR install/update infobar text did not match expectation",
                    expectedMessage, tempView.getText().toString());
            tempView = (TextView) decorView.findViewById(R.id.button_primary);
            Assert.assertEquals("VR install/update button text did not match expectation",
                    expectedButton, tempView.getText().toString());
        } else if (checkerReturnCompatibility
                == VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_SUPPORTED) {
            VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, false);
        } else {
            Assert.fail("Invalid VrCoreVersionChecker compatibility: "
                    + String.valueOf(checkerReturnCompatibility));
        }
        VrCoreInstallUtils.overrideVrCoreVersionChecker(null);
    }

    /**
     * Tests that the upgrade/install VR Services InfoBar is not present when VR Services is
     * installed and up to date.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testInfoBarNotPresentWhenVrServicesCurrent() {
        infoBarTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_READY);
    }

    /**
     * Tests that the upgrade VR Services InfoBar is present when VR Services is outdated.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA,
            XrActivityRestriction.SupportedActivity.CCT})
    public void
    testInfoBarPresentWhenVrServicesOutdated() {
        infoBarTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE);
    }

    /**
     * Tests that the install VR Services InfoBar is present when VR Services is missing.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA,
            XrActivityRestriction.SupportedActivity.CCT})
    public void
    testInfoBarPresentWhenVrServicesMissing() {
        infoBarTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services InfoBar is not present when VR is not supported on the
     * device.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testInfoBarNotPresentWhenVrServicesNotSupported() {
        infoBarTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_SUPPORTED);
    }
}
