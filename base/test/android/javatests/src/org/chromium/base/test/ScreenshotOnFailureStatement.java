// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;

import androidx.test.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;

import java.io.File;

/**
 * Statement that captures screenshots if |base| statement fails.
 *
 * If --screenshot-path commandline flag is given, this |Statement|
 * will save a screenshot to the specified path in the case of a test failure.
 */
public class ScreenshotOnFailureStatement extends Statement {
    private static final String TAG = "ScreenshotOnFail";

    private static final String EXTRA_SCREENSHOT_FILE =
            "org.chromium.base.test.ScreenshotOnFailureStatement.ScreenshotFile";

    private final Statement mBase;

    public ScreenshotOnFailureStatement(final Statement base) {
        mBase = base;
    }

    @Override
    public void evaluate() throws Throwable {
        try {
            mBase.evaluate();
        } catch (Throwable e) {
            takeScreenshot();
            throw e;
        }
    }

    private void takeScreenshot() {
        String screenshotFilePath =
                InstrumentationRegistry.getArguments().getString(EXTRA_SCREENSHOT_FILE);
        if (screenshotFilePath == null) {
            Log.d(
                    TAG,
                    String.format(
                            "Did not save screenshot of failure. Must specify %s "
                                    + "instrumentation argument to enable this feature.",
                            EXTRA_SCREENSHOT_FILE));
            return;
        }

        File screenshotFile = new File(screenshotFilePath);
        File screenshotDir = screenshotFile.getParentFile();
        if (screenshotDir == null) {
            Log.d(
                    TAG,
                    String.format(
                            "Failed to create parent directory for %s. Can't save screenshot.",
                            screenshotFile));
            return;
        }
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            if (!screenshotDir.exists()) {
                if (!screenshotDir.mkdirs()) {
                    Log.d(
                            TAG,
                            String.format(
                                    "Failed to create %s. Can't save screenshot.", screenshotDir));
                    return;
                }
            }

            // The Vega standalone VR headset can't take screenshots normally (they just show a
            // black screen with the VR overlay), so instead, use VrCore's RecorderService.
            if (Build.DEVICE.equals("vega")) {
                takeScreenshotVega(screenshotFile);
                return;
            }

            UiDevice uiDevice = null;
            try {
                uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
            } catch (RuntimeException ex) {
                Log.d(TAG, "Failed to initialize UiDevice", ex);
                return;
            }

            Log.d(TAG, String.format("Saving screenshot of test failure, %s", screenshotFile));
            uiDevice.takeScreenshot(screenshotFile);
        }
    }

    private void takeScreenshotVega(final File screenshotFile) {
        Intent screenshotIntent = new Intent();
        screenshotIntent.putExtra("command", "IMAGE");
        screenshotIntent.putExtra("quality", 100);
        screenshotIntent.putExtra("path", screenshotFile.toString());
        screenshotIntent.setComponent(
                new ComponentName(
                        "com.google.vr.vrcore",
                        "com.google.vr.vrcore.capture.record.RecorderService"));
        Log.d(TAG, String.format("Saving VR screenshot of test failure, %s", screenshotFile));
        InstrumentationRegistry.getContext().startService(screenshotIntent);
        // The screenshot taking is asynchronous, so wait until it actually gets taken before
        // returning, otherwise we can end up capturing the Daydream Home app instead of Chrome.
        // We can't use CriteriaHelper for polling since this is in base, and CriteriaHelper isn't.
        boolean screenshotSuccessful = false;
        // Poll for a second max with 10 attempts.
        for (int i = 0; i < 10; ++i) {
            if (screenshotFile.exists()) {
                screenshotSuccessful = true;
                break;
            }
            SystemClock.sleep(100);
        }
        if (!screenshotSuccessful) {
            Log.d(TAG, "Failed to save VR screenshot of test failure");
        }
    }
}
