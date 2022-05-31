// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID;

/** A helper class for showing PasswordSettings. */
public class PasswordManagerHelper {
    // Key for the argument with which PasswordsSettings will be launched. The value for
    // this argument should be part of the ManagePasswordsReferrer enum, which contains
    // all points of entry to the passwords settings.
    public static final String MANAGE_PASSWORDS_REFERRER = "manage-passwords-referrer";

    private static final String UPM_VARIATION_FEATURE_PARAM = "stage";

    // |PasswordSettings| full class name to open the fragment. Will be changed to
    // |PasswordSettings.class.getName()| once it's modularized.
    private static final String PASSWORD_SETTINGS_CLASS =
            "org.chromium.chrome.browser.password_manager.settings.PasswordSettings";
    private static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    private static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    private static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    private static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    private static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    private static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    private static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    private static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    private static final String PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Latency";
    private static final String PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Success";
    private static final String PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Error";
    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    public static boolean usesUnifiedPasswordManagerUI() {
        if (!ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ANDROID)) return false;
        @UpmExperimentVariation
        int variation = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                UNIFIED_PASSWORD_MANAGER_ANDROID, UPM_VARIATION_FEATURE_PARAM,
                UpmExperimentVariation.ENABLE_FOR_SYNCING_USERS);
        switch (variation) {
            case UpmExperimentVariation.ENABLE_FOR_SYNCING_USERS:
            case UpmExperimentVariation.ENABLE_FOR_ALL_USERS:
                return true;
            case UpmExperimentVariation.SHADOW_SYNCING_USERS:
            case UpmExperimentVariation.ENABLE_ONLY_BACKEND_FOR_SYNCING_USERS:
                return false;
        }
        assert false : "Whether to use UI is undefined for variation: " + variation;
        return false;
    }

    private static void recordFailureMetrics(
            @CredentialManagerError int error, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;
        final String kGetIntentErrorHistogram =
                forAccount ? ACCOUNT_GET_INTENT_ERROR_HISTOGRAM : LOCAL_GET_INTENT_ERROR_HISTOGRAM;
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, false);
        RecordHistogram.recordEnumeratedHistogram(
                kGetIntentErrorHistogram, error, CredentialManagerError.COUNT);
    }

    private static boolean launchIntent(PendingIntent intent) {
        boolean launchIntentSuccessfully = true;
        try {
            intent.send();
        } catch (CanceledException e) {
            launchIntentSuccessfully = false;
        }
        return launchIntentSuccessfully;
    }

    private static void launchCredentialManagerIntent(
            PendingIntent intent, long startTimeMs, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        recordSuccessMetrics(SystemClock.elapsedRealtime() - startTimeMs, forAccount);

        boolean launchIntentSuccessfully = launchIntent(intent);
        RecordHistogram.recordBooleanHistogram(forAccount
                        ? ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM
                        : LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                launchIntentSuccessfully);
    }

    private static void launchPasswordCheckup(PendingIntent intent, long startTimeMs) {
        RecordHistogram.recordTimesHistogram(PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM,
                SystemClock.elapsedRealtime() - startTimeMs);
        RecordHistogram.recordBooleanHistogram(PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, true);

        boolean launchIntentSuccessfully = launchIntent(intent);
        RecordHistogram.recordBooleanHistogram(
                PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                launchIntentSuccessfully);
    }

    private static void recordSuccessMetrics(long elapsedTimeMs, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        final String kGetIntentLatencyHistogram = forAccount ? ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM
                                                             : LOCAL_GET_INTENT_LATENCY_HISTOGRAM;
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;

        RecordHistogram.recordTimesHistogram(kGetIntentLatencyHistogram, elapsedTimeMs);
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, true);
    }
}
