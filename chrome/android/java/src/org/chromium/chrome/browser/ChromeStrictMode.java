// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ApplicationErrorReport;
import android.os.Build;
import android.os.Looper;
import android.os.StrictMode;

import androidx.annotation.UiThread;

import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Initialize application-level StrictMode reporting.
 */
public class ChromeStrictMode {
    private static final String TAG = "ChromeStrictMode";
    private static final double UPLOAD_PROBABILITY = 0.01;
    private static final double MAX_UPLOADS_PER_SESSION = 3;

    private static boolean sIsStrictModeAlreadyConfigured;
    private static List<Object> sCachedStackTraces =
            Collections.synchronizedList(new ArrayList<Object>());
    private static AtomicInteger sNumUploads = new AtomicInteger();

    private static class SnoopingArrayList<T> extends ArrayList<T> {
        @Override
        public void clear() {
            for (int i = 0; i < size(); i++) {
                // It is likely that we have at most one violation pass this check each time around.
                if (Math.random() < UPLOAD_PROBABILITY) {
                    // Ensure that we do not upload too many StrictMode violations in any single
                    // session. To prevent races, we allow sNumUploads to increase beyond the
                    // limit, but just skip actually uploading the stack trace then.
                    if (sNumUploads.getAndAdd(1) >= MAX_UPLOADS_PER_SESSION) {
                        break;
                    }
                    sCachedStackTraces.add(get(i));
                }
            }
            super.clear();
        }
    }

    /**
     * Always process the violation on the UI thread. This ensures other crash reports are not
     * corrupted. Since each individual user has a very small chance of uploading each violation,
     * and we have a hard cap of 3 per session, this will not affect performance too much.
     *
     * @param violationInfo The violation info from the StrictMode violation in question.
     */
    @UiThread
    private static void reportStrictModeViolation(Object violationInfo) {
        try {
            Field crashInfoField = violationInfo.getClass().getField("crashInfo");
            ApplicationErrorReport.CrashInfo crashInfo =
                    (ApplicationErrorReport.CrashInfo) crashInfoField.get(violationInfo);
            String stackTrace = crashInfo.stackTrace;
            if (stackTrace == null) {
                Log.d(TAG, "StrictMode violation stack trace was null.");
            } else {
                Log.d(TAG, "Upload stack trace: " + stackTrace);
                JavaExceptionReporter.reportStackTrace(stackTrace);
            }
        } catch (Exception e) {
            // Ignore all exceptions.
            Log.d(TAG, "Could not handle observed StrictMode violation.", e);
        }
    }

    /**
     * Replace Android OS's StrictMode.violationsBeingTimed with a custom ArrayList acting as an
     * observer into violation stack traces. Set up an idle handler so StrictMode violations that
     * occur on startup are not ignored.
     */
    @SuppressWarnings({"unchecked", "rawtypes" })
    @UiThread
    private static void initializeStrictModeWatch() {
        try {
            Field violationsBeingTimedField =
                    StrictMode.class.getDeclaredField("violationsBeingTimed");
            violationsBeingTimedField.setAccessible(true);
            ThreadLocal<ArrayList> violationsBeingTimed =
                    (ThreadLocal<ArrayList>) violationsBeingTimedField.get(null);
            ArrayList replacementList = new SnoopingArrayList();
            violationsBeingTimed.set(replacementList);
        } catch (Exception e) {
            // Terminate watch if any exceptions are raised.
            Log.w(TAG, "Could not initialize StrictMode watch.", e);
            return;
        }
        sNumUploads.set(0);
        // Delay handling StrictMode violations during initialization until the main loop is idle.
        Looper.myQueue().addIdleHandler(() -> {
            // Will retry if the native library has not been initialized.
            if (!LibraryLoader.getInstance().isInitialized()) return true;
            // Check again next time if no more cached stack traces to upload, and we have not
            // reached the max number of uploads for this session.
            if (sCachedStackTraces.isEmpty()) {
                // TODO(wnwen): Add UMA count when this happens.
                // In case of races, continue checking an extra time (equal condition).
                return sNumUploads.get() <= MAX_UPLOADS_PER_SESSION;
            }
            // Since this is the only place we are removing elements, no need for additional
            // synchronization to ensure it is still non-empty.
            reportStrictModeViolation(sCachedStackTraces.remove(0));
            return true;
        });
    }

    private static void turnOnDetection(StrictMode.ThreadPolicy.Builder threadPolicy,
            StrictMode.VmPolicy.Builder vmPolicy) {
        threadPolicy.detectAll();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            vmPolicy.detectAll();
        } else {
            // Explicitly enable detection of all violations except file URI leaks, as that
            // results in false positives when file URI intents are passed between Chrome
            // activities in separate processes. See http://crbug.com/508282#c11.
            vmPolicy.detectActivityLeaks()
                    .detectLeakedClosableObjects()
                    .detectLeakedRegistrationObjects()
                    .detectLeakedSqlLiteObjects();
        }
    }

    private static void addDefaultPenalties(StrictMode.ThreadPolicy.Builder threadPolicy,
            StrictMode.VmPolicy.Builder vmPolicy) {
        threadPolicy.penaltyLog().penaltyFlashScreen().penaltyDeathOnNetwork();
        vmPolicy.penaltyLog();
    }

    private static void addThreadDeathPenalty(StrictMode.ThreadPolicy.Builder threadPolicy) {
        threadPolicy.penaltyDeath();
    }

    private static void addVmDeathPenalty(StrictMode.VmPolicy.Builder vmPolicy) {
        vmPolicy.penaltyDeath();
    }

    /**
     * Turn on StrictMode detection based on build and command-line switches.
     */
    @UiThread
    // FindBugs doesn't like conditionals with compile time results
    public static void configureStrictMode() {
        assert ThreadUtils.runningOnUiThread();
        if (sIsStrictModeAlreadyConfigured) {
            return;
        }
        sIsStrictModeAlreadyConfigured = true;

        StrictMode.ThreadPolicy.Builder threadPolicy =
                new StrictMode.ThreadPolicy.Builder(StrictMode.getThreadPolicy());
        StrictMode.VmPolicy.Builder vmPolicy =
                new StrictMode.VmPolicy.Builder(StrictMode.getVmPolicy());

        CommandLine commandLine = CommandLine.getInstance();
        if ("eng".equals(Build.TYPE)
                || BuildConfig.DCHECK_IS_ON
                || ChromeVersionInfo.isLocalBuild()
                || commandLine.hasSwitch(ChromeSwitches.STRICT_MODE)) {
            turnOnDetection(threadPolicy, vmPolicy);
            addDefaultPenalties(threadPolicy, vmPolicy);
            if ("death".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                addThreadDeathPenalty(threadPolicy);
                addVmDeathPenalty(vmPolicy);
            } else if ("testing".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                addThreadDeathPenalty(threadPolicy);
                // Currently VmDeathPolicy kills the process, and is not visible on bot test output.
            }
        }
        // Enroll 1% of dev sessions into StrictMode watch. This is done client-side rather than
        // through finch because this decision is as early as possible in the browser initialization
        // process. We need to detect early start-up StrictMode violations before loading native and
        // before warming the SharedPreferences (that is a violation in an of itself). We will
        // closely monitor this on dev channel.
        boolean enableStrictModeWatch =
                (ChromeVersionInfo.isDevBuild() && Math.random() < UPLOAD_PROBABILITY);
        if ((ChromeVersionInfo.isLocalBuild() && !BuildConfig.DCHECK_IS_ON)
                || enableStrictModeWatch) {
            turnOnDetection(threadPolicy, vmPolicy);
            initializeStrictModeWatch();
        }

        StrictMode.setThreadPolicy(threadPolicy.build());
        StrictMode.setVmPolicy(vmPolicy.build());
    }
}
