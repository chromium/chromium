// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class should contain helpers for recording Password Manager metrics. */
@NullMarked
public class PasswordMetricsUtil {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        HistogramExportResult.SUCCESS,
        HistogramExportResult.WRITE_FAILED,
        HistogramExportResult.NO_CONSUMER,
        HistogramExportResult.ACTIVITY_DESTROYED,
        HistogramExportResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HistogramExportResult {
        int SUCCESS = 0;
        // Deprecated: int USER_ABORTED = 1;
        int WRITE_FAILED = 2;
        int NO_CONSUMER = 3;
        // Deprecated: int NO_SCREEN_LOCK_SET_UP = 4;
        int ACTIVITY_DESTROYED = 5;
        // If you add new values to HistogramExportResult, also update NUM_ENTRIES to match
        // its new size.
        int NUM_ENTRIES = 6;
    }

    // The prefix for the histograms, which will be used log the export flow metrics when the export
    // flow starts from password settings.
    public static final String PASSWORD_SETTINGS_EXPORT_METRICS_ID =
            "PasswordManager.Settings.Export";

    public static final String EXPORT_RESULT_HISTOGRAM_SUFFIX = ".Result2";

    public static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    public static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    public static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    public static final String ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError";
    public static final String ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError.ConnectionResultCode";
    public static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    public static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    public static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    public static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    public static final String LOCAL_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.APIError";
    public static final String LOCAL_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.APIError"
                    + ".ConnectionResultCode";
    public static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    public static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";

    public static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    /**
     * This is a helper that logs the results of password export which could be triggered from
     * Chrome settings or from the password migration warning.
     *
     * @param callerMetricsId indicates from which code path the password export was started
     * @param result is the value to be recorded
     */
    public static void logPasswordsExportResult(
            @Nullable String callerMetricsId, @HistogramExportResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                callerMetricsId + EXPORT_RESULT_HISTOGRAM_SUFFIX,
                result,
                HistogramExportResult.NUM_ENTRIES);
    }
}
