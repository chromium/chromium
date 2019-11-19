// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.VrBrowserTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;

import android.content.Intent;
import android.net.Uri;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.VrIntentDelegate;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.vr.VrShell;
import org.chromium.chrome.browser.vr.VrShellDelegate;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Class containing utility functions for transitioning between different states in the VR Browser.
 */
public class VrBrowserTransitionUtils extends VrTransitionUtils {
    /**
     * Forces Chrome into the VR Browser.
     *
     * @return True if the request to enter the VR Browser succeeded, false otherwise.
     */
    public static boolean forceEnterVrBrowser() {
        Boolean result = false;
        try {
            result = TestThreadUtils.runOnUiThreadBlocking(
                    () -> { return VrShellDelegate.enterVrIfNecessary(); });
        } catch (ExecutionException e) {
        }
        return result;
    }

    /**
     * Forces Chrome into the VR Browser, causing a test failure if it is not entered in the
     * allotted time.
     *
     * @param timeoutMs The amount of time in milliseconds to wait for VR Browser entry before
     *            failing.
     */
    public static void forceEnterVrBrowserOrFail(int timeoutMs) {
        Assert.assertTrue("Request to enter VR Browser failed", forceEnterVrBrowser());
        waitForVrEntry(timeoutMs);
    }

    /**
     * @return Whether the VR Browser's back button is enabled.
     */
    public static Boolean isBackButtonEnabled() {
        final AtomicBoolean isBackButtonEnabled = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            isBackButtonEnabled.set(
                    TestVrShellDelegate.getVrShellForTesting().isBackButtonEnabled());
        });
        return isBackButtonEnabled.get();
    }

    /**
     * @return Whether the VR Browers's forward button is enabled.
     */
    public static Boolean isForwardButtonEnabled() {
        final AtomicBoolean isForwardButtonEnabled = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            isForwardButtonEnabled.set(
                    TestVrShellDelegate.getVrShellForTesting().isForwardButtonEnabled());
        });
        return isForwardButtonEnabled.get();
    }

    /**
     * Navigates the VR Browser back.
     */
    public static void navigateBack() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TestVrShellDelegate.getVrShellForTesting().navigateBack(); });
    }

    /**
     * Navigates the VR Browser forward.
     */
    public static void navigateForward() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TestVrShellDelegate.getVrShellForTesting().navigateForward(); });
    }

    /**
     * Sends an intent to Chrome telling it to launch in VR mode.
     *
     * @param url String containing the URL to open.
     */
    public static void sendVrLaunchIntent(String url) {
        // Create an intent that will launch Chrome at the specified URL.
        final Intent intent =
                new Intent(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.addCategory(VrIntentDelegate.DAYDREAM_CATEGORY);
        VrModuleProvider.getIntentDelegate().setupVrIntent(intent);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { VrShellDelegate.getVrDaydreamApi().launchInVr(intent); });
    }

    /**
     * Sends an intent to Chrome telling it to open a tab in 2D mode.
     */
    public static void send2dMainIntent() {
        final Intent intent =
                new Intent(ContextUtils.getApplicationContext(), ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ContextUtils.getApplicationContext().startActivity(intent); });
    }

    /**
     * Waits until either a JavaScript dialog or permission prompt is being displayed using the
     * Android native UI in the VR browser.
     *
     * @param timeout How long in milliseconds to wait before timing out and failing.
     */
    public static void waitForNativeUiPrompt(final int timeout) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
            return vrShell.isDisplayingDialogView();
        }, "Native UI prompt did not display", timeout, POLL_CHECK_INTERVAL_SHORT_MS);
    }
}
