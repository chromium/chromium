// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.metrics.ChangeMetricsReportingStateCalledFrom;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;

/** Provides first run related utility functions. */
public class FirstRunUtils {
    private static Boolean sHasGoogleAccountAuthenticator;
    private static final int DEFAULT_SKIP_TOS_EXIT_DELAY_MS = 1000;
    private static final int A11Y_DELAY_FACTOR = 2;

    private static boolean sDisableDelayOnExitFreForTest;

    /**
     * Synchronizes first run native and Java preferences.
     * Must be called after native initialization.
     */
    public static void cacheFirstRunPrefs() {
        SharedPreferencesManager javaPrefs = SharedPreferencesManager.getInstance();
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
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
    }

    /**
     * Sets the EULA/Terms of Services state as "ACCEPTED".
     * @param allowMetricsAndCrashUploading True if the user allows to upload crash dumps and
     *         collect stats.
     */
    static void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        UmaSessionStats.changeMetricsReportingConsent(
                allowMetricsAndCrashUploading, ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
        setEulaAccepted();
    }

    /**
     * Determines whether or not the user has a Google account (so we can sync) or can add one.
     * @return Whether or not sync is allowed on this device.
     */
    static boolean canAllowSync() {
        return (hasGoogleAccountAuthenticator() && hasSyncPermissions()) || hasGoogleAccounts();
    }

    @VisibleForTesting
    static boolean hasGoogleAccountAuthenticator() {
        if (sHasGoogleAccountAuthenticator == null) {
            AccountManagerFacade accountHelper = AccountManagerFacadeProvider.getInstance();
            sHasGoogleAccountAuthenticator = accountHelper.hasGoogleAccountAuthenticator();
        }
        return sHasGoogleAccountAuthenticator;
    }

    @VisibleForTesting
    static void resetHasGoogleAccountAuthenticator() {
        sHasGoogleAccountAuthenticator = null;
    }

    @VisibleForTesting
    static boolean hasGoogleAccounts() {
        return !AccountUtils
                        .getAccountsIfFulfilledOrEmpty(
                                AccountManagerFacadeProvider.getInstance().getAccounts())
                        .isEmpty();
    }

    @SuppressLint("InlinedApi")
    private static boolean hasSyncPermissions() {
        UserManager manager = (UserManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USER_SERVICE);
        Bundle userRestrictions = manager.getUserRestrictions();
        return !userRestrictions.getBoolean(UserManager.DISALLOW_MODIFY_ACCOUNTS, false);
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public static boolean isFirstRunEulaAccepted() {
        return FirstRunUtilsJni.get().getFirstRunEulaAccepted();
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
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
     * {@link #DEFAULT_SKIP_TOS_EXIT_DELAY_MS}, while in a11y mode it will be extended by a factor
     * of {@link #A11Y_DELAY_FACTOR}. This is intended to avoid screen reader being interrupted, but
     * it is likely not going to work perfectly for all languages.
     *
     * @return The number of ms delay before exiting FRE with policy.
     */
    public static int getSkipTosExitDelayMs() {
        if (sDisableDelayOnExitFreForTest) return 0;

        int durationMs = DEFAULT_SKIP_TOS_EXIT_DELAY_MS;
        if (ChromeAccessibilityUtil.get().isTouchExplorationEnabled()) {
            durationMs *= A11Y_DELAY_FACTOR;
        }
        return durationMs;
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
