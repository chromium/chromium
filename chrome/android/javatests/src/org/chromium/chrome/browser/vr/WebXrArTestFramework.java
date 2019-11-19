// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * WebXR for AR-specific implementation of the WebXrTestFramework.
 */
public class WebXrArTestFramework extends WebXrTestFramework {
    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before).
     */
    public WebXrArTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Requests an AR session, automatically giving consent when prompted.
     * Causes a test failure if it is unable to do so, or if the consent prompt is missing.
     *
     * @param webContents The Webcontents to start the AR session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);

        enterSessionWithUserGesture(webContents);

        // We expect a session consent prompt (in this case the AR-specific one), but should not
        // get prompted for page camera permission.
        if (shouldExpectConsentDialog()) {
            PermissionUtils.waitForConsentPrompt(getRule().getActivity());
            PermissionUtils.acceptConsentPrompt(getRule().getActivity());
        }

        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.AR].currentSession != null",
                POLL_TIMEOUT_LONG_MS, webContents);
    }

    /**
     * Requests an AR session, then declines consent when prompted.
     * Causes a test failure if there was no prompt, or if the session started without consent.
     *
     * @param webContents The Webcontents to start the AR session in.
     */
    public void enterSessionWithUserGestureAndDeclineConsentOrFail(WebContents webContents) {
        if (!shouldExpectConsentDialog()) {
            Assert.fail("Attempted to decline the consent dialog when it would not appear.");
        }
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);

        enterSessionWithUserGesture(webContents);

        // We expect the AR-specific AR session consent prompt but should not get
        // prompted for page camera permission.
        PermissionUtils.waitForConsentPrompt(getRule().getActivity());
        PermissionUtils.declineConsentPrompt(getRule().getActivity());

        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].error != null", POLL_TIMEOUT_LONG_MS, webContents);
        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.AR].currentSession == null",
                POLL_TIMEOUT_LONG_MS, webContents);
    }

    /**
     * Exits a WebXR AR session.
     *
     * @param webcontents The WebContents to exit the AR session in
     */
    @Override
    public void endSession(WebContents webContents) {
        // Use a long timeout for session.end(), this can unexpectedly take more than
        // a second. TODO(https://crbug.com/1014159): investigate why.
        runJavaScriptOrFail("sessionInfos[sessionTypes.AR].currentSession.end()",
                POLL_TIMEOUT_LONG_MS, webContents);

        // Wait for the session to end before proceeding with followup tests.
        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.AR].currentSession == null",
                POLL_TIMEOUT_LONG_MS, webContents);
    }

    /**
     * Checks whether an immersive AR session would trigger the consent dialog.
     *
     * @param webContents The WebContents to check in.
     * @return True if an immersive AR session request would trigger the consent dialog, otherwise
     *     false.
     */
    @Override
    public boolean shouldExpectConsentDialog(WebContents webContents) {
        return shouldExpectConsentDialog("sessionTypes.AR", webContents);
    }
}
