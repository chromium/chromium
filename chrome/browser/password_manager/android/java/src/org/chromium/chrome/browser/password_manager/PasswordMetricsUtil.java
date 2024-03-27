// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class should contain helpers for recording Password Manager metrics. */
public class PasswordMetricsUtil {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PasswordMigrationWarningUserActions.GOT_IT,
        PasswordMigrationWarningUserActions.MORE_OPTIONS,
        PasswordMigrationWarningUserActions.SYNC,
        PasswordMigrationWarningUserActions.EXPORT,
        PasswordMigrationWarningUserActions.CANCEL,
        PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION,
        PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS,
        PasswordMigrationWarningUserActions.DISMISS_EMPTY_SHEET_OBSOLETE,
        PasswordMigrationWarningUserActions.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordMigrationWarningUserActions {
        int GOT_IT = 0;
        int MORE_OPTIONS = 1;
        int SYNC = 2;
        int EXPORT = 3;
        int CANCEL = 4;
        int DISMISS_INTRODUCTION = 5;
        int DISMISS_MORE_OPTIONS = 6;
        int DISMISS_EMPTY_SHEET_OBSOLETE = 7;
        int COUNT = 8;
    }

    public static final String PASSWORD_MIGRATION_WARNING_USER_ACTIONS =
            "PasswordManager.PasswordMigrationWarning.UserAction";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PostPasswordMigrationSheetOutcome.GOT_IT,
        PostPasswordMigrationSheetOutcome.DISMISS,
        PostPasswordMigrationSheetOutcome.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PostPasswordMigrationSheetOutcome {
        int GOT_IT = 0;
        int DISMISS = 1;
        int COUNT = 2;
    }

    public static final String POST_PASSWORD_MIGRATION_SHEET_OUTCOME =
            "PasswordManager.PostPasswordsMigrationSheet.Outcome";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PasswordMigrationWarningSheetStateAtClosing.FULL_SHEET_CLOSED,
        PasswordMigrationWarningSheetStateAtClosing.EMPTY_SHEET_CLOSED_BY_USER_INTERACTION,
        PasswordMigrationWarningSheetStateAtClosing.EMPTY_SHEET_CLOSED_WITHOUT_USER_INTERACTION,
        PasswordMigrationWarningSheetStateAtClosing.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordMigrationWarningSheetStateAtClosing {
        int FULL_SHEET_CLOSED = 0;
        int EMPTY_SHEET_CLOSED_BY_USER_INTERACTION = 1;
        int EMPTY_SHEET_CLOSED_WITHOUT_USER_INTERACTION = 2;
        int COUNT = 3;
    }

    public static final String PASSWORD_MIGRATION_WARNING_SHEET_STATE_AT_CLOSING =
            "PasswordManager.PasswordMigrationWarning.SheetStateAtClosing";
    public static final String PASSWORD_MIGRATION_WARNING_EMPTY_SHEET_TRIGGER =
            "PasswordManager.PasswordMigrationWarning.EmptySheetTrigger2";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        HistogramExportResult.SUCCESS,
        HistogramExportResult.USER_ABORTED,
        HistogramExportResult.WRITE_FAILED,
        HistogramExportResult.NO_CONSUMER,
        HistogramExportResult.NO_SCREEN_LOCK_SET_UP,
        HistogramExportResult.ACTIVITY_DESTROYED,
        HistogramExportResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HistogramExportResult {
        int SUCCESS = 0;
        int USER_ABORTED = 1;
        int WRITE_FAILED = 2;
        int NO_CONSUMER = 3;
        int NO_SCREEN_LOCK_SET_UP = 4;
        int ACTIVITY_DESTROYED = 5;
        // If you add new values to HistogramExportResult, also update NUM_ENTRIES to match
        // its new size.
        int NUM_ENTRIES = 6;
    }

    // The prefix for the histograms, which will be used log the export flow metrics when the export
    // flow starts form the password migration warning.
    public static final String PASSWORD_MIGRATION_WARNING_EXPORT_METRICS_ID =
            "PasswordManager.PasswordMigrationWarning.Export";

    // The prefix for the histograms, which will be used log the export flow metrics when the export
    // flow starts form the password migration warning.
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
     * This is a helper that logs that the user has taken on the password migration warning sheet.
     * @param result is the value to be recorded
     */
    public static void logPasswordMigrationWarningUserAction(
            @PasswordMigrationWarningUserActions int result) {
        RecordHistogram.recordEnumeratedHistogram(
                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                result,
                PasswordMigrationWarningUserActions.COUNT);
    }

    /**
     * This is a helper that logs what happened with the post password migration sheet such that it
     * got closed.
     *
     * @param result is the value to be recorded
     */
    public static void logPostPasswordMigrationOutcome(
            @PostPasswordMigrationSheetOutcome int result) {
        RecordHistogram.recordEnumeratedHistogram(
                POST_PASSWORD_MIGRATION_SHEET_OUTCOME,
                result,
                PostPasswordMigrationSheetOutcome.COUNT);
    }

    /**
     * This is a helper that logs the results of password export which could be triggered from
     * Chrome settings or from the password migration warning.
     *
     * @param callerMetricsId indicates from which code path the password export was started
     * @param result is the value to be recorded
     */
    public static void logPasswordsExportResult(
            String callerMetricsId, @HistogramExportResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                callerMetricsId + EXPORT_RESULT_HISTOGRAM_SUFFIX,
                result,
                HistogramExportResult.NUM_ENTRIES);
    }
}
