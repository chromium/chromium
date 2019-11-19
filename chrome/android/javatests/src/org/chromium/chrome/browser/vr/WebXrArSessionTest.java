// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.WebXrArTestFramework.POLL_TIMEOUT_SHORT_MS;

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
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.XrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.WebContents;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for testing WebXR for AR's requestSession behavior.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXR,WebXRARModule,LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.N) // WebXR for AR is only supported on N+
public class WebXrArSessionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            XrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArSessionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = XrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /**
     * Tests that a session request for AR succeeds.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testArRequestSessionSucceeds() {
        mWebXrArTestFramework.loadUrlAndAwaitInitialization(
                mWebXrArTestFramework.getEmbeddedServerUrlForHtmlTestFile(
                        "test_ar_request_session_succeeds"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that declining AR consent causes future attempts to still display the consent dialog,
     * but consenting causes future attempts to skip the consent dialog as long as no navigation
     * occurs.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testConsentPersistanceOnSamePage() {
        mWebXrArTestFramework.loadUrlAndAwaitInitialization(
                mWebXrArTestFramework.getEmbeddedServerUrlForHtmlTestFile(
                        "test_ar_request_session_succeeds"),
                PAGE_LOAD_TIMEOUT_S);
        WebContents contents = mWebXrArTestFramework.getCurrentWebContents();

        // Start session, decline consent prompt.
        mWebXrArTestFramework.enterSessionWithUserGestureAndDeclineConsentOrFail(contents);
        mWebXrArTestFramework.assertNoJavaScriptErrors();

        // Start new session, accept consent prompt this time.
        PermissionUtils.waitForConsentPromptDismissal(mTestRule.getActivity());
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(contents);
        mWebXrArTestFramework.endSession();
        mWebXrArTestFramework.assertNoJavaScriptErrors();

        // Start yet another session, but go through a path that doesn't automatically handle
        // the consent dialog to ensure that it doesn't actually appear.
        mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].currentSession == null", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.enterSessionWithUserGesture();
        mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].currentSession != null", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that repeatedly starting and stopping AR sessions does not cause any unexpected
     * behavior. Regression test for https://crbug.com/837894.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testRepeatedArSessionsSucceed() {
        mWebXrArTestFramework.loadUrlAndAwaitInitialization(
                mWebXrArTestFramework.getEmbeddedServerUrlForHtmlTestFile(
                        "test_ar_request_session_succeeds"),
                PAGE_LOAD_TIMEOUT_S);
        for (int i = 0; i < 2; i++) {
            mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
            mWebXrArTestFramework.endSession();
        }
        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }
}
