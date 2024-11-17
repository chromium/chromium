// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Build;
import android.os.Looper;
import android.os.StrictMode;
import android.text.TextUtils;

import androidx.annotation.UiThread;

import org.chromium.base.CommandLine;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.strictmode.StrictModePolicyViolation;
import org.chromium.components.strictmode.Violation;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Initialize application-level StrictMode reporting. */
public class ChromeStrictMode {
    private static final String TAG = "ChromeStrictMode";
    private static final double UPLOAD_PROBABILITY = 0.01;
    private static final double MAX_UPLOADS_PER_SESSION = 3;

    private static boolean sIsStrictModeAlreadyConfigured;
    private static List<Violation> sCachedViolations =
            Collections.synchronizedList(new ArrayList<>());
    private static AtomicInteger sNumUploads = new AtomicInteger();

    /**
     * Always process the violation on the UI thread. This ensures other crash reports are not
     * corrupted. Since each individual user has a very small chance of uploading each violation,
     * and we have a hard cap of 3 per session, this will not affect performance too much.
     *
     * @param violation The violation info from the StrictMode violation in question.
     */
    @UiThread
    private static void reportStrictModeViolation(Violation violation) {
        StringWriter stackTraceWriter = new StringWriter();
        new StrictModePolicyViolation(violation).printStackTrace(new PrintWriter(stackTraceWriter));
        String stackTrace = stackTraceWriter.toString();
        if (TextUtils.isEmpty(stackTrace)) {
            Log.d(TAG, "StrictMode violation stack trace was empty.");
        } else {
            Log.d(TAG, "Upload stack trace: " + stackTrace);
            JavaExceptionReporter.reportStackTrace(stackTrace);
        }
    }

    /** Set up an idle handler so StrictMode violations that occur on startup are not ignored. */
    @UiThread
    private static void initializeStrictModeWatch() {
        sNumUploads.set(0);
        // Delay handling StrictMode violations during initialization until the main loop is idle.
        Looper.myQueue()
                .addIdleHandler(
                        () -> {
                            // Will retry if the native library has not been initialized.
                            if (!LibraryLoader.getInstance().isInitialized()) return true;
                            // Check again next time if no more cached stack traces to upload, and
                            // we have not reached the max number of uploads for this session.
                            if (sCachedViolations.isEmpty()) {
                                // TODO(wnwen): Add UMA count when this happens.
                                // In case of races, continue checking an extra time (equal
                                // condition).
                                return sNumUploads.get() <= MAX_UPLOADS_PER_SESSION;
                            }
                            // Since this is the only place we are removing elements, no need for
                            // additional synchronization to ensure it is still non-empty.
                            reportStrictModeViolation(sCachedViolations.remove(0));
                            return true;
                        });
    }

    private static void turnOnDetection(StrictMode.VmPolicy.Builder vmPolicy) {
        // Do not enable detectUntaggedSockets(). It does not support native (the vast majority
        // of our sockets), and we have not bothered to tag our Java uses.
        // https://crbug.com/770792
        // Also: Do not enable detectCleartextNetwork(). We can't prevent websites from using
        // non-https.
        vmPolicy.detectActivityLeaks()
                .detectLeakedClosableObjects()
                .detectLeakedRegistrationObjects()
                .detectLeakedSqlLiteObjects();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Introduced in O.
            vmPolicy.detectContentUriWithoutPermission();
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Introduced in Q.
            vmPolicy.detectCredentialProtectedWhileLocked().detectImplicitDirectBoot();
        }

        // File URI leak detection, has false positives when file URI intents are passed between
        // Chrome activities in separate processes. See http://crbug.com/508282#c11.
        vmPolicy.detectFileUriExposure();
    }

    private static void addDefaultVmPenalties(StrictMode.VmPolicy.Builder vmPolicy) {
        vmPolicy.penaltyLog();
    }

    private static void addVmDeathPenalty(StrictMode.VmPolicy.Builder vmPolicy) {
        vmPolicy.penaltyDeath();
    }

    /** Turn on StrictMode detection based on build and command-line switches. */
    @UiThread
    // FindBugs doesn't like conditionals with compile time results
    public static void configureStrictMode() {
        assert ThreadUtils.runningOnUiThread();
        if (sIsStrictModeAlreadyConfigured) {
            return;
        }
        sIsStrictModeAlreadyConfigured = true;

        // Check is present so that proguard deletes the strict mode detection code for non-local
        // non-dev-channel release builds. This is needed because ChromeVersionInfo#isLocalBuild()
        // is not listed with -assumenosideeffects in the proguard configuration
        if (!ChromeStrictModeSwitch.ALLOW_STRICT_MODE_CHECKING) return;

        CommandLine commandLine = CommandLine.getInstance();
        boolean shouldApplyPenalties =
                BuildConfig.ENABLE_ASSERTS
                        || VersionInfo.isLocalBuild()
                        || commandLine.hasSwitch(ChromeSwitches.STRICT_MODE);

        // Enroll 1% of dev sessions into StrictMode watch. This is done client-side rather than
        // through finch because this decision is as early as possible in the browser initialization
        // process. We need to detect early start-up StrictMode violations before loading native and
        // before warming the SharedPreferences (that is a violation in an of itself). We will
        // closely monitor this on dev channel.
        boolean enableStrictModeWatch =
                (VersionInfo.isLocalBuild() && !BuildConfig.ENABLE_ASSERTS)
                        || (VersionInfo.isDevBuild() && Math.random() < UPLOAD_PROBABILITY);
        if (!shouldApplyPenalties && !enableStrictModeWatch) return;

        StrictMode.VmPolicy.Builder vmPolicy =
                new StrictMode.VmPolicy.Builder(StrictMode.getVmPolicy());
        turnOnDetection(vmPolicy);

        if (shouldApplyPenalties) {
            addDefaultVmPenalties(vmPolicy);
            if ("death".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                addVmDeathPenalty(vmPolicy);
            } else if ("testing".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                // Currently VmDeathPolicy kills the process, and is not visible on bot test output.
            }
        }
        if (enableStrictModeWatch) {
            initializeStrictModeWatch();
        }

        StrictMode.setVmPolicy(vmPolicy.build());
    }
}
