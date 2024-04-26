// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/** WebXR for AR-specific implementation of the WebXrTestFramework. */
public class WebXrArTestFramework extends WebXrTestFramework {
    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is tagged
     * with @Before).
     */
    public WebXrArTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Requests an AR session, automatically granting permission when prompted. Causes a test
     * failure if it is unable to do so, or if the permission prompt is missing.
     *
     * @param webContents The Webcontents to start the AR session in.
     * @param needsCameraPermission True if the session requires Camera permission.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(
            WebContents webContents, boolean needsCameraPermission) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);

        boolean willPromptForCamera =
                needsCameraPermission && permissionRequestWouldTriggerPrompt("camera");

        enterSessionWithUserGesture(webContents);

        // We expect a session permissiom prompt (in this case the AR-specific one):
        if (shouldExpectPermissionPrompt()) {
            PermissionUtils.waitForPermissionPrompt();
            PermissionUtils.acceptPermissionPrompt();
        }

        if (willPromptForCamera) {
            PermissionUtils.waitForPermissionPrompt();
            PermissionUtils.acceptPermissionPrompt();
        }

        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].currentSession != null",
                POLL_TIMEOUT_LONG_MS,
                webContents);
    }

    /**
     * Exits a WebXR AR session.
     *
     * @param webcontents The WebContents to exit the AR session in
     */
    @Override
    public void endSession(WebContents webContents) {
        // Use a long timeout for session.end(), this can unexpectedly take more than
        // a second. TODO(crbug.com/40653025): investigate why.
        runJavaScriptOrFail(
                "sessionInfos[sessionTypes.AR].currentSession.end()",
                POLL_TIMEOUT_LONG_MS,
                webContents);

        // Wait for the session to end before proceeding with followup tests.
        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.AR].currentSession == null",
                POLL_TIMEOUT_LONG_MS,
                webContents);
    }

    /**
     * Checks whether an immersive AR session would trigger the permission prompt.
     *
     * @param webContents The WebContents to check in.
     * @return True if an immersive AR session request would trigger the permission prompt,
     *     otherwise false.
     */
    @Override
    public boolean shouldExpectPermissionPrompt(WebContents webContents) {
        return shouldExpectPermissionPrompt("sessionTypes.AR", webContents);
    }
}
