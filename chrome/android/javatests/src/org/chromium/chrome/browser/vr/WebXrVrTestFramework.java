// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Extension of VrTestFramework containing WebXR for VR-specific functionality. */
public class WebXrVrTestFramework extends WebXrTestFramework {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        PERMISSION_PROMPT_ACTION_DO_NOTHING,
        PERMISSION_PROMPT_ACTION_ALLOW,
        PERMISSION_PROMPT_ACTION_DENY
    })
    public @interface PermissionPromptAction {}

    public static final int PERMISSION_PROMPT_ACTION_DO_NOTHING = 0;
    public static final int PERMISSION_PROMPT_ACTION_ALLOW = 1;
    public static final int PERMISSION_PROMPT_ACTION_DENY = 2;

    protected @PermissionPromptAction int mPermissionPromptAction = PERMISSION_PROMPT_ACTION_ALLOW;

    public WebXrVrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Set the default action to be taken when the permission prompt is displayed.
     *
     * @param action The action to take on a permission prompt.
     */
    public void setPermissionPromptAction(@PermissionPromptAction int action) {
        mPermissionPromptAction = action;
    }

    /**
     * VR-specific implementation of enterSessionWithUserGesture that includes a workaround for
     * receiving broadcasts late.
     *
     * @param webContents The WebContents for the tab to enter the VR session in.
     */
    @Override
    public void enterSessionWithUserGesture(WebContents webContents) {
        super.enterSessionWithUserGesture(webContents);

        if (!shouldExpectPermissionPrompt()) return;
        PermissionUtils.waitForPermissionPrompt();
        if (mPermissionPromptAction == PERMISSION_PROMPT_ACTION_ALLOW) {
            PermissionUtils.acceptPermissionPrompt();
        } else if (mPermissionPromptAction == PERMISSION_PROMPT_ACTION_DENY) {
            PermissionUtils.denyPermissionPrompt();
        }
    }

    /**
     * 'enterSessionWithUserGestureOrFail' is specific to immersive sessions. This method does the
     * same, but for the magic window session.
     */
    public void enterMagicWindowSessionWithUserGestureOrFail() {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.MAGIC_WINDOW", POLL_TIMEOUT_SHORT_MS);
        enterSessionWithUserGesture();
        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
                POLL_TIMEOUT_LONG_MS);
    }

    /**
     * WebXR for VR-specific implementation of enterSessionWithUserGestureOrFail.
     *
     * @param webContents The WebContents of the tab to enter the immersive session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(
            WebContents webContents, boolean needsCameraPermission) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.IMMERSIVE", POLL_TIMEOUT_LONG_MS, webContents);

        boolean willPromptForCamera =
                needsCameraPermission && permissionRequestWouldTriggerPrompt("camera");

        enterSessionWithUserGesture(webContents);

        if (willPromptForCamera) {
            PermissionUtils.waitForPermissionPrompt();
            PermissionUtils.acceptPermissionPrompt();
        }

        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
                POLL_TIMEOUT_LONG_MS,
                webContents);
    }

    /**
     * End an immersive session and wait until that session has actually ended.
     *
     * @param webContents The WebContents for the tab to end the session in.
     */
    @Override
    public void endSession(WebContents webContents) {
        // Use a long timeout for session.end(), this can unexpectedly take more than
        // a second. TODO(crbug.com/40653025): investigate why.
        runJavaScriptOrFail(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
                POLL_TIMEOUT_LONG_MS,
                webContents);

        // Wait for the session to end before proceeding with followup tests.
        pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
                POLL_TIMEOUT_LONG_MS,
                webContents);
    }

    /**
     * Checks whether an immersive VR session would trigger the permission prompt.
     *
     * @param webContents The WebContents to check in.
     * @return True if an immersive VR session request would trigger the permission prompt,
     *     otherwise false.
     */
    @Override
    public boolean shouldExpectPermissionPrompt(WebContents webContents) {
        return shouldExpectPermissionPrompt("sessionTypeToRequest", webContents);
    }
}
