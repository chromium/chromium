// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.Build;
import android.support.test.filters.MediumTest;

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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for various scenarios around when the consent dialog is expected.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=WebXR,LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
public class WebXrVrConsentTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrConsentTestFramework mWebXrVrConsentTestFramework;

    public WebXrVrConsentTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrConsentTestFramework = new WebXrVrConsentTestFramework(mTestRule);
    }

    /**
     * Tests that denying consent blocks the session from being created.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentCancelFailsSessionCreation() {
        mWebXrVrConsentTestFramework.setConsentDialogAction(
                WebXrVrTestFramework.CONSENT_DIALOG_ACTION_DENY);
        mWebXrVrConsentTestFramework.setConsentDialogExpected(true);

        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_consent"),
                PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.enterSessionWithUserGesture();
        mWebXrVrConsentTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.IMMERSIVE].error != null", POLL_TIMEOUT_LONG_MS);
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "verifySessionConsentError(sessionTypes.IMMERSIVE)", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that attempting to enter a session that requires the same permission level does not
     * reprompt.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentPersistsSameLevel() {
        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);

        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    /**
     * Tests that attempting to enter an inline session with no special features does not require
     * consent.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentNotNeededForInline() {
        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);

        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_consent"),
                PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "requestMagicWindowSession()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
                POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests that if consent is granted for a higher level, the lower level does not need consent.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentPersistsLowerLevel() {
        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_consent"),
                PAGE_LOAD_TIMEOUT_S);

        // Set up to request the highest level of consent support on Android (height).
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestHeight()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        // Now set up to request the lower level of consent. The session should be entered without
        // the consent prompt.
        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestDefault()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    /**
     * Tests that if consent is granted for a lower level, the higher level still needs consent.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentRepromptsHigherLevel() {
        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_consent"),
                PAGE_LOAD_TIMEOUT_S);

        // Request a session at the lowest level of consent, and ensure that it is entered.
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestDefault()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        // Now request a session that requires a higher level of consent. It should still be
        // prompted for consent and the session should enter.
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestHeight()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    /**
     * Tests that granted consent does not persist after a page reload.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentRepromptsAfterReload() {
        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        mWebXrVrConsentTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }
}
