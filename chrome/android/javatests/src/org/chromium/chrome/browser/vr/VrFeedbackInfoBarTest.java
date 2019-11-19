// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrInfoBarUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;

/**
 * Tests for the infobar that prompts the user to enter feedback on their VR browsing experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_SVR})
public class VrFeedbackInfoBarTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mTestRule = new ChromeTabbedActivityVrTestRule();

    private WebXrVrTestFramework mWebXrVrTestFramework;
    private VrBrowserTestFramework mVrBrowserTestFramework;

    private static final String TEST_PAGE_2D_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");
    private static final String TEST_PAGE_WEBXR_URL =
            WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page");

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mVrBrowserTestFramework = new VrBrowserTestFramework(mTestRule);
        Assert.assertFalse(
                "Test started opting out of feedback", VrFeedbackStatus.getFeedbackOptOut());
    }

    private void assertState(boolean isInVr, boolean isInfobarVisible) {
        Assert.assertEquals(
                "Browser VR state did not match expectation", isInVr, VrShellDelegate.isInVr());
        VrInfoBarUtils.expectInfoBarPresent(mTestRule, isInfobarVisible);
    }

    private void enterThenExitVr() {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        VrBrowserTransitionUtils.forceExitVr();
    }

    /**
     * Tests that we respect the feedback frequency when showing the feedback prompt.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testFeedbackFrequency() {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        // Set frequency of infobar to every 2nd time.
        VrShellDelegateUtils.getDelegateInstance().setFeedbackFrequencyForTesting(2);

        // Verify that the Feedback infobar is visible when exiting VR.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
        VrInfoBarUtils.clickInfobarCloseButton(mTestRule);

        // Feedback infobar shouldn't show up this time.
        enterThenExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);

        // Feedback infobar should show up again.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
        VrInfoBarUtils.clickInfobarCloseButton(mTestRule);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that we don't show the feedback prompt when the user has opted-out.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testFeedbackOptOut() {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        // Show infobar every time.
        VrShellDelegateUtils.getDelegateInstance().setFeedbackFrequencyForTesting(1);

        // The infobar should show the first time.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);

        // Opt-out of seeing the infobar.
        VrInfoBarUtils.clickInfoBarButton(VrInfoBarUtils.Button.SECONDARY, mTestRule);
        Assert.assertTrue("Did not opt out of VR feedback", VrFeedbackStatus.getFeedbackOptOut());

        // The infobar should not show because the user opted out.
        enterThenExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that we only show the feedback prompt when the user has actually used the VR browser.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testFeedbackOnlyOnVrBrowsing_WebXr() {
        feedbackOnlyOnVrBrowsingImpl(TEST_PAGE_WEBXR_URL, mWebXrVrTestFramework);
    }

    private void feedbackOnlyOnVrBrowsingImpl(String url, WebXrVrTestFramework framework) {
        // Enter VR presentation mode.
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        Assert.assertTrue("Did not enter WebVR presentation",
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());

        // Exiting VR should not prompt for feedback since the no VR browsing was performed.
        VrBrowserTransitionUtils.forceExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);
        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that we show the prompt if the VR browser is used after exiting a WebXR immersive
     * session.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
            public void testExitPresentationInVr_WebXr() {
        exitPresentationInVrImpl(TEST_PAGE_WEBXR_URL, mWebXrVrTestFramework);
    }

    private void exitPresentationInVrImpl(String url, final WebXrVrTestFramework framework) {
        // Enter VR presentation mode.
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        Assert.assertTrue("Did not enter WebVR presentation",
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());

        // Exit presentation mode by navigating to a different url.
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_2D_URL, () -> {
                    mVrBrowserTestFramework.runJavaScriptOrFail(
                            "window.location.href = '" + TEST_PAGE_2D_URL + "';",
                            POLL_TIMEOUT_SHORT_MS);
                }, POLL_TIMEOUT_LONG_MS);

        // Exiting VR should prompt for feedback since 2D browsing was performed after.
        VrBrowserTransitionUtils.forceExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
        framework.assertNoJavaScriptErrors();
    }
}
