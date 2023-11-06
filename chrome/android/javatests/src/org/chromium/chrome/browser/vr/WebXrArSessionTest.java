// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.WebXrArTestFramework.POLL_TIMEOUT_SHORT_MS;

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
import org.chromium.chrome.browser.modaldialog.ChromeModalDialogTestUtils;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** End-to-end tests for testing WebXR for AR's requestSession behavior. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXR,WebXRARModule,WebXRHitTest,LogJsConsoleMessages"
})
public class WebXrArSessionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArSessionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /** Tests that a session request for AR succeeds. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testArRequestSessionSucceeds() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_ar_request_session_succeeds", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that consenting causes future attempts to skip the permission prompt as long as no
     * navigation occurs.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testArPermissionPersistance() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_ar_request_session_succeeds", PAGE_LOAD_TIMEOUT_S);

        // Start new session, accepting the consent prompt
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ false);
        mWebXrArTestFramework.endSession();
        mWebXrArTestFramework.assertNoJavaScriptErrors();
        mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].currentSession == null", POLL_TIMEOUT_SHORT_MS);

        // Start yet another session, but go through a path that doesn't automatically handle
        // the permission prompt to ensure that it doesn't actually appear.
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
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_ar_request_session_succeeds", PAGE_LOAD_TIMEOUT_S);
        for (int i = 0; i < 2; i++) {
            mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
            mWebXrArTestFramework.endSession();
        }
        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that requesting a permission prompt during an AR Session shows the browser
     * controls/greys out the screen and doesn't exit the AR Session.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA})
    public void testPermissionRequestDuringAr() throws TimeoutException {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_permission_request_during_ar", PAGE_LOAD_TIMEOUT_S);
        WebContents contents = mWebXrArTestFramework.getCurrentWebContents();

        // Start new session, accepting the consent prompt
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ false);
        mWebXrArTestFramework.assertNoJavaScriptErrors();

        // Trigger a new permission prompt to show when tapping on the canvas, then tap on it.
        mWebXrArTestFramework.runJavaScriptOrFail("setupCanvasClick()", POLL_TIMEOUT_SHORT_MS);
        Assert.assertTrue(DOMUtils.clickNode(contents, "webgl-canvas"));
        PermissionUtils.waitForPermissionPrompt();

        // Now that the new permission prompt is showing, ensure that the browser controls have
        // shown themselves and that we still have an AR Session.
        ChromeModalDialogTestUtils.checkBrowserControls(mTestRule.getActivity(), true);
        Assert.assertTrue(
                Boolean.valueOf(
                        mWebXrArTestFramework.runJavaScriptOrFail(
                                "sessionInfos[sessionTypes.AR].currentSession != null",
                                POLL_TIMEOUT_SHORT_MS)));

        // Reject the permission prompt to hide it again.
        PermissionUtils.denyPermissionPrompt();
        PermissionUtils.waitForPermissionPromptDismissal();

        // Now that the new permission prompt is dismissed, ensure that the browser controls have
        // hidden themselves and that we still have an AR Session.
        ChromeModalDialogTestUtils.checkBrowserControls(mTestRule.getActivity(), false);
        Assert.assertTrue(
                Boolean.valueOf(
                        mWebXrArTestFramework.runJavaScriptOrFail(
                                "sessionInfos[sessionTypes.AR].currentSession != null",
                                POLL_TIMEOUT_SHORT_MS)));

        mWebXrArTestFramework.assertNoJavaScriptErrors();
    }
}
