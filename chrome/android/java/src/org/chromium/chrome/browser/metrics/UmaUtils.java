// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * Utilities to support startup metrics - Android version.
 */
@JNINamespace("chrome::android")
public class UmaUtils {
    private static boolean sRunningApplicationStart;

    // All these values originate from SystemClock.uptimeMillis().
    private static long sApplicationStartTimeMs;
    private static long sForegroundStartTimeMs;
    private static long sBackgroundTimeMs;

    /**
     * Record the time in the application lifecycle at which Chrome code first runs
     * (Application.attachBaseContext()).
     */
    @MainDex
    public static void recordMainEntryPointTime() {
        // We can't simply pass this down through a JNI call, since the JNI for chrome
        // isn't initialized until we start the native content browser component, and we
        // then need the start time in the C++ side before we return to Java. As such we
        // save it in a static that the C++ can fetch once it has initialized the JNI.
        sApplicationStartTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Record the time at which Chrome was brought to foreground.
     */
    public static void recordForegroundStartTime() {
        // Since this can be called from multiple places (e.g. ChromeActivitySessionTracker
        // and FirstRunActivity), only set the time if it hasn't been set previously or if
        // Chrome has been sent to background since the last foreground time.
        if (sForegroundStartTimeMs == 0 || sForegroundStartTimeMs < sBackgroundTimeMs) {
            sForegroundStartTimeMs = SystemClock.uptimeMillis();
        }
    }

    /**
     * Record the time at which Chrome was sent to background.
     */
    public static void recordBackgroundTime() {
        sBackgroundTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Determines if Chrome was brought to foreground.
     */
    public static boolean hasComeToForeground() {
        return sForegroundStartTimeMs != 0;
    }

    /**
     * Determines if Chrome was brought to background.
     */
    public static boolean hasComeToBackground() {
        return sBackgroundTimeMs != 0;
    }

    /**
     * Determines if this client is eligible to send metrics and crashes based on sampling. If it
     * is, and there was user consent, then metrics and crashes would be reported
     */
    public static boolean isClientInMetricsReportingSample() {
        return UmaUtilsJni.get().isClientInMetricsReportingSample();
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
    public static long getMainEntryPointTicks() {
        return sApplicationStartTimeMs;
    }

    public static long getForegroundStartTicks() {
        assert sForegroundStartTimeMs != 0;
        return sForegroundStartTimeMs;
    }

    @CalledByNative
    private static void setUsageAndCrashReportingFromNative(boolean enabled) {
        UmaSessionStats.changeMetricsReportingConsent(enabled);
    }

    @NativeMethods
    interface Natives {
        boolean isClientInMetricsReportingSample();
        void recordMetricsReportingDefaultOptIn(boolean optIn);
    }
}
