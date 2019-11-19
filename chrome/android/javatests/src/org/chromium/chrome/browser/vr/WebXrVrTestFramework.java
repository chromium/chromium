// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import androidx.annotation.IntDef;

import org.junit.Assert;

import org.chromium.chrome.browser.vr.rules.VrTestRule;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Extension of VrTestFramework containing WebXR for VR-specific functionality.
 */
public class WebXrVrTestFramework extends WebXrTestFramework {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({CONSENT_DIALOG_ACTION_DO_NOTHING, CONSENT_DIALOG_ACTION_ALLOW,
            CONSENT_DIALOG_ACTION_DENY})
    public @interface ConsentDialogAction {}

    public static final int CONSENT_DIALOG_ACTION_DO_NOTHING = 0;
    public static final int CONSENT_DIALOG_ACTION_ALLOW = 1;
    public static final int CONSENT_DIALOG_ACTION_DENY = 2;

    @ConsentDialogAction
    protected int mConsentDialogAction = CONSENT_DIALOG_ACTION_ALLOW;

    public WebXrVrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
        if (!TestVrShellDelegate.isOnStandalone()) {
            Assert.assertFalse("Test started in VR", VrShellDelegate.isInVr());
        }
    }

    /**
     * Set the default action to be taken when the consent dialog is displayed.
     *
     * @param action The action to take on a consent dialog.
     */
    public void setConsentDialogAction(@ConsentDialogAction int action) {
        mConsentDialogAction = action;
    }

    /**
     * VR-specific implementation of enterSessionWithUserGesture that includes a workaround for
     * receiving broadcasts late.
     *
     * @param webContents The WebContents for the tab to enter the VR session in.
     */
    @Override
    public void enterSessionWithUserGesture(WebContents webContents) {
        // TODO(https://crbug.com/762724): Remove this workaround when the issue with being resumed
        // before receiving the VR broadcast is fixed on VrCore's end.
        // However, we don't want to enable the workaround if the DON flow is enabled, as that
        // causes issues.
        if (!((VrTestRule) getRule()).isDonEnabled()) {
            VrShellDelegateUtils.getDelegateInstance().setExpectingBroadcast();
        }
        super.enterSessionWithUserGesture(webContents);

        if (!shouldExpectConsentDialog()) return;
        PermissionUtils.waitForConsentPrompt(getRule().getActivity());
        if (mConsentDialogAction == CONSENT_DIALOG_ACTION_ALLOW) {
            PermissionUtils.acceptConsentPrompt(getRule().getActivity());
        } else if (mConsentDialogAction == CONSENT_DIALOG_ACTION_DENY) {
            PermissionUtils.declineConsentPrompt(getRule().getActivity());
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
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.IMMERSIVE", POLL_TIMEOUT_LONG_MS, webContents);
        enterSessionWithUserGesture(webContents);

        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
                POLL_TIMEOUT_LONG_MS, webContents);
        Assert.assertTrue("Immersive session started, but VR Shell not in presentation mode",
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * End an immersive session and wait until that session has actually ended.
     *
     * @param webContents The WebContents for the tab to end the session in.
     */
    @Override
    public void endSession(WebContents webContents) {
        // Use a long timeout for session.end(), this can unexpectedly take more than
        // a second. TODO(https://crbug.com/1014159): investigate why.
        runJavaScriptOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
                POLL_TIMEOUT_LONG_MS, webContents);

        // Wait for the session to end before proceeding with followup tests.
        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
                POLL_TIMEOUT_LONG_MS, webContents);
    }

    /**
     * Checks whether an immersive VR session would trigger the consent dialog.
     *
     * @param webContents The WebContents to check in.
     * @return True if an immersive VR session request would trigger the consent dialog, otherwise
     *     false.
     */
    @Override
    public boolean shouldExpectConsentDialog(WebContents webContents) {
        return shouldExpectConsentDialog("sessionTypeToRequest", webContents);
    }
}
