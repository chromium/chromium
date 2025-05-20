// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.metrics.ChangeMetricsReportingStateCalledFrom;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.ui.accessibility.AccessibilityState;

/** Provides first run related utility functions. */
@NullMarked
public class FirstRunUtils {
    private static final int DEFAULT_SKIP_TOS_EXIT_DELAY_MS = 1000;

    private static boolean sDisableDelayOnExitFreForTest;

    /**
     * Synchronizes first run native and Java preferences. Must be called after native
     * initialization.
     */
    public static void cacheFirstRunPrefs() {
        // Backup and restore does not restore native pref, so this needs to update it. Note that
        // these prefs are slightly different, the eula is set when the ToS is accepted (early in
        // the FRE), while the FRE flow is only complete at the end.
        if (FirstRunStatus.getFirstRunFlowComplete() && !isFirstRunEulaAccepted()) {
            setEulaAccepted();
        }
    }

    /**
     * Sets the EULA/Terms of Services state as "ACCEPTED".
     *
     * @param allowMetricsAndCrashUploading True if the user allows to upload crash dumps and
     *     collect stats.
     */
    static void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        UmaSessionStats.changeMetricsReportingConsent(
                allowMetricsAndCrashUploading, ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN);
        setEulaAccepted();
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public static boolean isFirstRunEulaAccepted() {
        return FirstRunUtilsJni.get().getFirstRunEulaAccepted();
    }

    /** Sets the preference that signals when the user has accepted the EULA. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setEulaAccepted() {
        FirstRunUtilsJni.get().setEulaAccepted();
    }

    /**
     * @return Whether the ToS should be shown during the first-run for CCTs/PWAs.
     */
    public static boolean isCctTosDialogEnabled() {
        return FirstRunUtilsJni.get().getCctTosDialogEnabled();
    }

    /**
     * The the number of ms delay before exiting FRE with policy. By default the delay would be
     * {@link #DEFAULT_SKIP_TOS_EXIT_DELAY_MS}, but we will get the recommended timeout from the
     * AccessibilityState, which calculates a time based on currently running accessibility
     * services and OS-level system settings.
     *
     * @return The number of ms delay before exiting FRE with policy.
     */
    public static int getSkipTosExitDelayMs() {
        if (sDisableDelayOnExitFreForTest) return 0;

        return AccessibilityState.getRecommendedTimeoutMillis(
                DEFAULT_SKIP_TOS_EXIT_DELAY_MS, DEFAULT_SKIP_TOS_EXIT_DELAY_MS);
    }

    public static void setDisableDelayOnExitFreForTest(boolean isDisable) {
        sDisableDelayOnExitFreForTest = isDisable;
        ResettersForTesting.register(() -> sDisableDelayOnExitFreForTest = false);
    }

    @NativeMethods
    public interface Natives {
        boolean getFirstRunEulaAccepted();

        void setEulaAccepted();

        boolean getCctTosDialogEnabled();
    }
}
