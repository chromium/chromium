// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.metrics.ChangeMetricsReportingStateCalledFrom;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.accessibility.AccessibilityState;

/** Provides first run related utility functions. */
public class FirstRunUtils {
    private static final int DEFAULT_SKIP_TOS_EXIT_DELAY_MS = 1000;

    private static boolean sDisableDelayOnExitFreForTest;

    /**
     * Synchronizes first run native and Java preferences.
     * Must be called after native initialization.
     */
    public static void cacheFirstRunPrefs() {
        SharedPreferencesManager javaPrefs = ChromeSharedPreferences.getInstance();
        // Set both Java and native prefs if any of the three indicators indicate ToS has been
        // accepted. This needed because:
        //   - Old versions only set native pref, so this syncs Java pref.
        //   - Backup & restore does not restore native pref, so this needs to update it.
        //   - checkAnyUserHasSeenToS() may be true which needs to sync its state to the prefs.
        boolean javaPrefValue =
                javaPrefs.readBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
        boolean nativePrefValue = isFirstRunEulaAccepted();
        boolean isFirstRunComplete = FirstRunStatus.getFirstRunFlowComplete();
        if (javaPrefValue || nativePrefValue || isFirstRunComplete) {
            if (!javaPrefValue) {
                javaPrefs.writeBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
            }
            if (!nativePrefValue) {
                setEulaAccepted();
            }
        }
    }

    /**
     * @return Whether the user has accepted Chrome Terms of Service.
     */
    public static boolean didAcceptTermsOfService() {
        // Note: Does not check FirstRunUtils.isFirstRunEulaAccepted() because this may be called
        // before native is initialized.
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
    }

    /**
     * Sets the EULA/Terms of Services state as "ACCEPTED".
     * @param allowMetricsAndCrashUploading True if the user allows to upload crash dumps and
     *         collect stats.
     */
    static void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        UmaSessionStats.changeMetricsReportingConsent(
                allowMetricsAndCrashUploading, ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
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
