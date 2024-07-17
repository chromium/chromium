// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.os.SystemClock;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Extension of XrTestFramework meant for testing XR-related web APIs. */
public abstract class WebXrTestFramework extends XrTestFramework {
    private static final String TAG = "WebXrTestFramework";

    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is tagged
     * with @Before).
     */
    public WebXrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * WebVrTestFramework derives from this and overrides to allow WebVR tests to fail early if no
     * VRDisplay's were found. WebXR has no concept of a device, and inline support is always
     * available, so return true.
     *
     * @param webContents The WebContents to run the JavaScript through.
     * @return Whether an XRDevice was found.
     */
    public boolean xrDeviceFound(WebContents webContents) {
        return true;
    }

    /**
     * Helper function to run xrDeviceFound with the current tab's WebContents.
     *
     * @return Whether an XRDevice was found.
     */
    public boolean xrDeviceFound() {
        return xrDeviceFound(getCurrentWebContents());
    }

    /**
     * Enters a WebXR or WebVR session of some kind by tapping on the canvas on the page. Needs to
     * be non-static despite not using any member variables in order for the WebContents-less helper
     * version to work properly in subclasses.
     *
     * @param webContents The WebContents for the tab the canvas is in.
     */
    public void enterSessionWithUserGesture(WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "enterSessionWithUserGesture");

        // This method includes multiple workarounds, see https://crbug.com/c/998307 for
        // context. In short, canvas clicks sometimes don't register after a transition
        // from a WebXR immersive session to VR Browser mode. Sometimes clickNode returns
        // false, but even when it returns true the click event doesn't always get
        // processed by JavaScript. Use a JavaScript variable to verify if the click was
        // received, and retry if it wasn't.
        boolean canvasClicked = false;
        runJavaScriptOrFail("canvasClicked=false", POLL_TIMEOUT_SHORT_MS, webContents);
        for (int i = 0; i < 3; ++i) {
            if (i > 0) {
                Log.e(TAG, "Failed to click canvas: retry #" + i);
            }
            boolean nodeClicked = false;
            try {
                nodeClicked =
                        DOMUtils.clickNode(
                                webContents, "webgl-canvas", /* goThroughRootAndroidView= */ false);
                if (DEBUG_LOGS) {
                    Log.i(TAG, "enterSessionWithUserGesture: nodeClicked => " + nodeClicked);
                }
                if (!nodeClicked) {
                    Log.e(TAG, "Failed to click canvas: clickNode is false");
                    // Since this path didn't involve a timeout, wait a bit before retrying.
                    SystemClock.sleep(1000);
                }
            } catch (TimeoutException e) {
                Log.e(TAG, "Failed to click canvas: " + e.toString());
            }
            if (nodeClicked) {
                canvasClicked = pollJavaScriptBoolean("canvasClicked", POLL_TIMEOUT_SHORT_MS);
                if (!canvasClicked) {
                    Log.e(TAG, "Failed to click canvas: canvasClicked is false");
                }
            } else {
                // nodeClicked is false, retry.
                continue;
            }

            if (canvasClicked) break;

            PropertyModel dialog =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    getRule()
                                            .getActivity()
                                            .getModalDialogManager()
                                            .getCurrentDialogForTest());

            // If we get here, "nodeClicked" is true but "canvasClicked" is false. Before
            // retrying, check if there's a dialog visible. Polling Javascript doesn't
            // work while a dialog is showing, and sometimes the click is handled quickly
            // enough for the consent prompt to show before we got a chance to check the
            // canvasClicked variable. In that case, assume we got the click and continue.
            if (dialog != null) {
                if (DEBUG_LOGS) Log.i(TAG, "Got a dialog, stop waiting for click");
                canvasClicked = true;
                break;
            }
        }

        if (!canvasClicked) {
            Assert.fail("Failed to click canvas to enter session: Click not received");
        }
    }

    /** Helper function to run enterSessionWithUserGesture using the current tab's WebContents. */
    public void enterSessionWithUserGesture() {
        enterSessionWithUserGesture(getCurrentWebContents());
    }

    /**
     * Enters a WebXR or WebVR session of some kind and waits until the page reports it is finished
     * with its JavaScript step. Needs to be non-static despite not using any member variables in
     * order for the WebContents-less helper version to work properly in subclasses.
     *
     * @param webContents The WebContents for the tab to enter the session in.
     */
    public void enterSessionWithUserGestureAndWait(WebContents webContents) {
        enterSessionWithUserGesture(webContents);
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Helper function to run enterSessionWithUserGestureAndWait with the current tab's WebContents.
     */
    public void enterSessionWithUserGestureAndWait() {
        enterSessionWithUserGestureAndWait(getCurrentWebContents());
    }

    /**
     * Attempts to enter a WebXR or WebVR session of some kind, failing if it is unable to.
     *
     * @param webContents The WebContents for the tab to enter the session in.
     * @param needsCameraPermission True if the session requires Camera permission.
     */
    public abstract void enterSessionWithUserGestureOrFail(
            WebContents webContents, boolean needsCameraPermission);

    /**
     * Helper function to run enterSessionWithUserGestureOrFail with the current tab's WebContents.
     * Session will be treated as not requiring Camera permission.
     */
    public void enterSessionWithUserGestureOrFail() {
        enterSessionWithUserGestureOrFail(false);
    }

    /**
     * Helper function to run enterSessionWithUserGestureOrFail with the current tab's WebContents.
     *
     * @param needsCameraPermission True if the session requires Camera permission.
     */
    public void enterSessionWithUserGestureOrFail(boolean needsCameraPermission) {
        enterSessionWithUserGestureOrFail(getCurrentWebContents(), needsCameraPermission);
    }

    /**
     * Ends whatever type of session a subclass enters with enterSessionWithUserGesture.
     *
     * @param webContents The WebContents to end the session in.
     */
    public abstract void endSession(WebContents webContents);

    /** Helper function to run endSession with the current tab's WebContents. */
    public void endSession() {
        endSession(getCurrentWebContents());
    }

    /**
     * Helper function to run shouldExpectPermissionPrompt with the correct session type for the
     * framework.
     *
     * @param webContents The WebContents to check the permission prompt in.
     * @return True if the a request for the session type would trigger the permission prompt to be
     *     shown, otherwise false.
     */
    public abstract boolean shouldExpectPermissionPrompt(WebContents webContents);

    /**
     * Helper function to run shouldExpectPermissionPrompt with the current tab's WebContents.
     *
     * @return True if the a request for the session type would trigger the permission prompt to be
     *     shown, otherwise false.
     */
    public boolean shouldExpectPermissionPrompt() {
        return shouldExpectPermissionPrompt(getCurrentWebContents());
    }

    /**
     * Checks whether a session request of the given type is expected to trigger the consent dialog.
     *
     * @param sessionType The session type to pass to JavaScript defined in webxr_boilerplate.js,
     *     e.g. sessionTypes.AR
     * @param webContents The WebContents to check in.
     * @return True if the given session type is expected to trigger the permission prompt,
     *     otherwise false.
     */
    protected boolean shouldExpectPermissionPrompt(String sessionType, WebContents webContents) {
        return runJavaScriptOrFail(
                        "sessionTypeWouldTriggerConsent(" + sessionType + ")",
                        POLL_TIMEOUT_SHORT_MS,
                        webContents)
                .equals("true");
    }
}
