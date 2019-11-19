// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
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
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
public class WebXrVrTabTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrTabTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
    }

    /**
     * Tests that non-focused tabs don't get WebXR rAFs called. Disabled on standalones because
     * they will always be in the VR Browser, and thus shouldn't be getting inline poses even
     * if the tab is focused.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_SVR)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPoseDataUnfocusedTab_WebXr() {
        testPoseDataUnfocusedTabImpl(WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                                             "webxr_test_pose_data_unfocused_tab"),
                mWebXrVrTestFramework);
    }

    private void testPoseDataUnfocusedTabImpl(String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepCheckFrameDataWhileFocusedTab()");
        WebContents firstTabContents = framework.getCurrentWebContents();

        mTestRule.loadUrlInNewTab("about:blank");

        WebXrVrTestFramework.executeStepAndWait(
                "stepCheckFrameDataWhileNonFocusedTab()", firstTabContents);
        WebXrVrTestFramework.endTest(firstTabContents);
    }

    /**
     * Tests that permissions in use by other tabs are shown while in a WebXR for VR immersive
     * session.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPermissionsInOtherTab() throws InterruptedException {
        testPermissionsInOtherTabImpl(false /* incognito */);
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPermissionsInOtherTabIncognito() throws InterruptedException {
        testPermissionsInOtherTabImpl(true /* incognito */);
    }

    private void testPermissionsInOtherTabImpl(boolean incognito) throws InterruptedException {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                mWebXrVrTestFramework.getEmbeddedServerUrlForHtmlTestFile(
                        "generic_webxr_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Be sure to store the stream we're given so that the permission is actually in use, as
        // otherwise the toast doesn't show up since another tab isn't actually using the
        // permission.
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "requestPermission({audio:true}, true /* storeValue */)", POLL_TIMEOUT_SHORT_MS);

        // Accept the permission prompt. Standalone devices need to be special cased since they
        // will be in the VR Browser.
        if (TestVrShellDelegate.isOnStandalone()) {
            NativeUiUtils.enableMockedInput();
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {});
            NativeUiUtils.waitForUiQuiescence();
            NativeUiUtils.clickFallbackUiPositiveButton();
        } else {
            PermissionUtils.waitForPermissionPrompt();
            PermissionUtils.acceptPermissionPrompt();
        }

        mWebXrVrTestFramework.waitOnJavaScriptStep();

        if (incognito) {
            mWebXrVrTestFramework.openIncognitoTab("about:blank");
        } else {
            mTestRule.loadUrlInNewTab("about:blank");
        }

        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.WEB_XR_AUDIO_INDICATOR, true /* visible */, () -> {});
    }
}
