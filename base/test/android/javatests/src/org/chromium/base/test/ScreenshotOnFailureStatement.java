// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import androidx.core.view.OneShotPreDrawListener;
import androidx.test.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Statement that captures screenshots if |base| statement fails.
 *
 * <p>If --screenshot-path commandline flag is given, this |Statement| will save a screenshot to the
 * specified path in the case of a test failure.
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

            // Make screenshots work on incognito windows.
            clearFlagSecureAndWaitForApply();

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

    private static void clearFlagSecureAndWaitForApply() {
        ThreadUtils.assertOnBackgroundThread();
        // Clearing FLAG_SECURE only updates the window's LayoutParams. The change is sent to the
        // WindowManager on the next ViewRootImpl traversal, and is then applied asynchronously by
        // WindowManager / SurfaceFlinger. Wait for both, or else the screenshot will still be
        // black.
        CountDownLatch latch = new CountDownLatch(1);
        boolean anySecure =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            View decorView = null;
                            for (Activity activity : ActivityFinisher.snapshotActivities()) {
                                Window window = activity.getWindow();
                                View decor = window.getDecorView();
                                boolean isSecure =
                                        (window.getAttributes().flags
                                                        & WindowManager.LayoutParams.FLAG_SECURE)
                                                != 0;
                                if (!isSecure || !decor.isAttachedToWindow()) {
                                    continue;
                                }
                                decorView = decor;
                                window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
                            }
                            if (decorView == null) {
                                return false;
                            }
                            // If multiple windows were secure, waiting on one is sufficient since
                            // their traversals happen in the same frame.
                            OneShotPreDrawListener.add(decorView, latch::countDown);
                            return true;
                        });
        if (!anySecure) {
            return;
        }
        try {
            // Guards against a hung UI thread. The sync below is still worth attempting.
            if (!latch.await(1, TimeUnit.SECONDS)) {
                Log.d(TAG, "Timed out waiting for pre-draw after clearing FLAG_SECURE.");
            }
        } catch (InterruptedException e) {
            Log.d(TAG, "Interrupted waiting for pre-draw after clearing FLAG_SECURE.");
        }
        // The WindowManager applies the change on its own surface placement pass, which then
        // needs to be committed by SurfaceFlinger. There is no public API to wait for this
        // directly, but UiAutomation.injectInputEvent(sync=true) performs exactly this sync (see
        // WindowManagerService.syncInputTransactions()) regardless of whether the event is
        // accepted. Use an orphan key-up event, which InputDispatcher drops as inconsistent, so
        // that the app never sees it.
        KeyEvent event = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_UNKNOWN);
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .injectInputEvent(event, /* sync= */ true);
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
