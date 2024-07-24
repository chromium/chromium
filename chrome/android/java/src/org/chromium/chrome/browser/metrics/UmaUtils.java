// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.ActivityManager;
import android.app.usage.UsageStatsManager;
import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

/** Utilities to support startup metrics - Android version. */
@JNINamespace("chrome::android")
public class UmaUtils {
    /** Observer for this class. */
    public interface Observer {
        /**
         * Called when hasComeToForeground() changes from false to true for the first time after
         * post-native initialization has started.
         */
        void onHasComeToForegroundWithNative();
    }

    private static Observer sObserver;

    /** Sets the observer. */
    public static void setObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        assert sObserver == null;
        sObserver = observer;
    }

    /** Removes the observer. */
    public static void removeObserver() {
        ThreadUtils.assertOnUiThread();
        sObserver = null;
    }

    // All these values originate from SystemClock.uptimeMillis().
    private static long sApplicationStartTimeMs;
    private static long sForegroundStartWithNativeTimeMs;
    private static long sBackgroundWithNativeTimeMs;

    private static boolean sSkipRecordingNextForegroundStartTimeForTesting;

    // Will short-circuit out of the next recordForegroundStartTimeWithNative() call.
    public static void skipRecordingNextForegroundStartTimeForTesting() {
        sSkipRecordingNextForegroundStartTimeForTesting = true;
    }

    /**
     * App standby bucket status, used for UMA reporting. Enum values correspond to the return
     * values of {@link UsageStatsManager#getAppStandbyBucket}.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({
        StandbyBucketStatus.ACTIVE,
        StandbyBucketStatus.WORKING_SET,
        StandbyBucketStatus.FREQUENT,
        StandbyBucketStatus.RARE,
        StandbyBucketStatus.RESTRICTED,
        StandbyBucketStatus.UNSUPPORTED,
        StandbyBucketStatus.EXEMPTED,
        StandbyBucketStatus.NEVER,
        StandbyBucketStatus.OTHER,
        StandbyBucketStatus.COUNT
    })
    private @interface StandbyBucketStatus {
        int ACTIVE = 0;
        int WORKING_SET = 1;
        int FREQUENT = 2;
        int RARE = 3;
        int RESTRICTED = 4;
        int UNSUPPORTED = 5;
        int EXEMPTED = 6;
        int NEVER = 7;
        int OTHER = 8;
        int COUNT = 9;
    }

    /**
     * Record the time in the application lifecycle at which Chrome code first runs
     * (Application.attachBaseContext()).
     */
    public static void recordMainEntryPointTime() {
        // We can't simply pass this down through a JNI call, since the JNI for chrome
        // isn't initialized until we start the native content browser component, and we
        // then need the start time in the C++ side before we return to Java. As such we
        // save it in a static that the C++ can fetch once it has initialized the JNI.
        sApplicationStartTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Record the time at which Chrome was brought to foreground. Should be recorded only after
     * post-native initialization has started.
     *
     * A notable exception is FRE. It records foreground time in OnResume(), which can happen before
     * native. It was made in 2016 to allow native initialization in FRE without errors. See
     * http://crrev.com/436530.
     */
    public static void recordForegroundStartTimeWithNative() {
        if (sSkipRecordingNextForegroundStartTimeForTesting) {
            sSkipRecordingNextForegroundStartTimeForTesting = false;
            return;
        }

        // Since this can be called from multiple places (e.g. ChromeActivitySessionTracker
        // and FirstRunActivity), only set the time if it hasn't been set previously or if
        // Chrome has been sent to background since the last foreground time.
        if (sForegroundStartWithNativeTimeMs == 0
                || sForegroundStartWithNativeTimeMs < sBackgroundWithNativeTimeMs) {
            if (sObserver != null && sForegroundStartWithNativeTimeMs == 0) {
                sObserver.onHasComeToForegroundWithNative();
            }
            sForegroundStartWithNativeTimeMs = SystemClock.uptimeMillis();
        }
    }

    /**
     * Record the time at which Chrome was sent to background.
     *
     * Should not be called before post-native initialization.
     */
    public static void recordBackgroundTimeWithNative() {
        sBackgroundWithNativeTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Determines whether Chrome was brought to foreground after post-native initialization started.
     */
    public static boolean hasComeToForegroundWithNative() {
        return sForegroundStartWithNativeTimeMs != 0;
    }

    /** Determines if Chrome was brought to background. */
    public static boolean hasComeToBackgroundWithNative() {
        return sBackgroundWithNativeTimeMs != 0;
    }

    /**
     * Determines if this client is eligible to send metrics based on sampling. If it is, and there
     * was user consent, then metrics should be reported.
     */
    public static boolean isClientInSampleForMetrics() {
        return UmaUtilsJni.get().isClientInSampleForMetrics();
    }

    /**
     * Determines if this client is eligible to send crashes based on sampling. If it is, and there
     * was user consent, then crashes should be reported.
     */
    public static boolean isClientInSampleForCrashes() {
        return UmaUtilsJni.get().isClientInSampleForCrashes();
    }

    /** Records various levels of background restrictions imposed by android on chrome. */
    public static void recordBackgroundRestrictions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;
        Context context = ContextUtils.getApplicationContext();
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        boolean isBackgroundRestricted = activityManager.isBackgroundRestricted();
        RecordHistogram.recordBooleanHistogram(
                "Android.BackgroundRestrictions.IsBackgroundRestricted", isBackgroundRestricted);

        int standbyBucketUma = getStandbyBucket(context);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BackgroundRestrictions.StandbyBucket",
                standbyBucketUma,
                StandbyBucketStatus.COUNT);

        if (isBackgroundRestricted) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.BackgroundRestrictions.StandbyBucket.WithUserRestriction",
                    standbyBucketUma,
                    StandbyBucketStatus.COUNT);
        }
    }

    /** Record minidump uploading time split by background restriction status. */
    public static void recordMinidumpUploadingTime(long taskDurationMs) {
        RecordHistogram.recordCustomTimesHistogram(
                "Stability.Android.MinidumpUploadingTime",
                taskDurationMs,
                1,
                DateUtils.DAY_IN_MILLIS,
                50);
    }

    private static @StandbyBucketStatus int getStandbyBucket(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return StandbyBucketStatus.UNSUPPORTED;

        UsageStatsManager usageStatsManager =
                (UsageStatsManager) context.getSystemService(Context.USAGE_STATS_SERVICE);
        int standbyBucket = usageStatsManager.getAppStandbyBucket();
        int standbyBucketUma;
        switch (standbyBucket) {
            case UsageStatsManager.STANDBY_BUCKET_ACTIVE:
                standbyBucketUma = StandbyBucketStatus.ACTIVE;
                break;
            case UsageStatsManager.STANDBY_BUCKET_WORKING_SET:
                standbyBucketUma = StandbyBucketStatus.WORKING_SET;
                break;
            case UsageStatsManager.STANDBY_BUCKET_FREQUENT:
                standbyBucketUma = StandbyBucketStatus.FREQUENT;
                break;
            case UsageStatsManager.STANDBY_BUCKET_RARE:
                standbyBucketUma = StandbyBucketStatus.RARE;
                break;
            case UsageStatsManager.STANDBY_BUCKET_RESTRICTED:
                standbyBucketUma = StandbyBucketStatus.RESTRICTED;
                break;
            case 5: // STANDBY_BUCKET_EXEMPTED
                standbyBucketUma = StandbyBucketStatus.EXEMPTED;
                break;
            case 50: // STANDBY_BUCKET_NEVER
                standbyBucketUma = StandbyBucketStatus.NEVER;
                break;
            default:
                standbyBucketUma = StandbyBucketStatus.OTHER;
                break;
        }
        return standbyBucketUma;
    }

    /**
     * Sets whether metrics reporting was opt-in or not. If it was opt-in, then the enable checkbox
     * on first-run was default unchecked. If it was opt-out, then the checkbox was default checked.
     * This should only be set once, and only during first-run.
     */
    public static void recordMetricsReportingDefaultOptIn(boolean optIn) {
        UmaUtilsJni.get().recordMetricsReportingDefaultOptIn(optIn);
    }

    @CalledByNative
    public static long getApplicationStartTime() {
        return sApplicationStartTimeMs;
    }

    @CalledByNative
    public static long getProcessStartTime() {
        return Process.getStartUptimeMillis();
    }

    @NativeMethods
    interface Natives {
        boolean isClientInSampleForMetrics();

        boolean isClientInSampleForCrashes();

        void recordMetricsReportingDefaultOptIn(boolean optIn);
    }
}
