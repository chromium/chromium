// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.mock.MockGvrVrCoreVersionChecker;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;
import org.chromium.chrome.browser.vr.util.VrMessageUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.vr.R;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * End-to-end tests for the message that prompts the user to update or install VrCore (VR Services)
 * when attempting to use a VR feature with an outdated or entirely missing version or other
 * VR-related update prompts.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=LogJsConsoleMessages"
})
public class GvrInstallUpdateMessageTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            GvrTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;

    public GvrInstallUpdateMessageTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = GvrTestRuleUtils.wrapRuleInActivityRestrictionRule(mVrTestRule);
    }

    /**
     * Creates and sets a MockGvrVrCoreVersionCheckerImpl as the VrShellDelegate's
     * VrCoreVersionChecker instance.
     *
     * @param compatibility An int corresponding to a VrCoreCompatibility value that the mock
     *     version checker will return.
     * @return The MockGvrVrCoreVersionCheckerImpl that was set as VrShellDelegate's
     *     VrCoreVersionChecker instance.
     */
    private static MockGvrVrCoreVersionChecker setVrCoreCompatibility(int compatibility) {
        final MockGvrVrCoreVersionChecker mockChecker = new MockGvrVrCoreVersionChecker();
        mockChecker.setMockReturnValue(compatibility);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VrCoreInstallUtils.overrideVrCoreVersionChecker(mockChecker);
                });
        Assert.assertEquals(
                "Overriding VrCoreVersionChecker failed",
                compatibility,
                mockChecker.getLastReturnValue());
        return mockChecker;
    }

    /**
     * Helper function to run the tests checking for the upgrade/install message being present since
     * all that differs is the value returned by VrCoreVersionChecker and a couple asserts.
     *
     * @param checkerReturnCompatibility The compatibility to have the VrCoreVersionChecker return.
     */
    private void messageTestHelper(final int checkerReturnCompatibility) throws ExecutionException {
        VrCoreInstallUtils vrCoreInstallUtils = VrCoreInstallUtils.create(0);
        setVrCoreCompatibility(checkerReturnCompatibility);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    vrCoreInstallUtils.requestInstallVrCore(
                            mVrTestRule.getActivity().getCurrentWebContents());
                });
        if (checkerReturnCompatibility == VrCoreVersionChecker.VrCoreCompatibility.VR_READY) {
            VrMessageUtils.expectMessagePresent(mVrTestRule, false);
        } else if (checkerReturnCompatibility
                        == VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE
                || checkerReturnCompatibility
                        == VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_AVAILABLE) {
            // Out of date and missing cases are the same, but with different text
            String expectedTitle;
            String expectedButton;
            Context context = ContextUtils.getApplicationContext();
            if (checkerReturnCompatibility
                    == VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE) {
                expectedTitle = context.getString(R.string.vr_services_check_message_update_title);
                expectedButton =
                        context.getString(R.string.vr_services_check_message_update_button);
            } else {
                expectedTitle = context.getString(R.string.vr_services_check_message_install_title);
                expectedButton =
                        context.getString(R.string.vr_services_check_message_install_button);
            }
            PropertyModel message = VrMessageUtils.getVrInstallUpdateMessage(mVrTestRule);
            Assert.assertNotNull("VR install/update message should be present.", message);

            Assert.assertEquals(
                    "VR install/update message text did not match expectation.",
                    expectedTitle,
                    message.get(MessageBannerProperties.TITLE));
            Assert.assertEquals(
                    "VR install/update message description did not match expectation.",
                    context.getString(R.string.vr_services_check_message_description),
                    message.get(MessageBannerProperties.DESCRIPTION));
            Assert.assertEquals(
                    "VR install/update message button text did not match expectation.",
                    expectedButton,
                    message.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        } else if (checkerReturnCompatibility
                == VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_SUPPORTED) {
            VrMessageUtils.expectMessagePresent(mVrTestRule, false);
        } else {
            Assert.fail(
                    "Invalid VrCoreVersionChecker compatibility: " + checkerReturnCompatibility);
        }
        VrCoreInstallUtils.overrideVrCoreVersionChecker(null);
    }

    /**
     * Tests that the upgrade/install VR Services message is not present when VR Services is
     * installed and up to date.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testMessageNotPresentWhenVrServicesCurrent() throws ExecutionException {
        messageTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_READY);
    }

    /** Tests that the upgrade VR Services message is present when VR Services is outdated. */
    @Test
    @MediumTest
    @XrActivityRestriction({
        XrActivityRestriction.SupportedActivity.CTA,
        XrActivityRestriction.SupportedActivity.CCT
    })
    public void testMessagePresentWhenVrServicesOutdated() throws ExecutionException {
        messageTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_OUT_OF_DATE);
    }

    /** Tests that the install VR Services message is present when VR Services is missing. */
    @Test
    @MediumTest
    @XrActivityRestriction({
        XrActivityRestriction.SupportedActivity.CTA,
        XrActivityRestriction.SupportedActivity.CCT
    })
    public void testMessagePresentWhenVrServicesMissing() throws ExecutionException {
        messageTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services message is not present when VR is not supported on the
     * device.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testMessageNotPresentWhenVrServicesNotSupported() throws ExecutionException {
        messageTestHelper(VrCoreVersionChecker.VrCoreCompatibility.VR_NOT_SUPPORTED);
    }
}
