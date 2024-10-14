// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Used by the access loss warning UI to log metrics. */
public class AccessLossWarningMetricsRecorder {

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PasswordAccessLossWarningUserAction.MAIN_ACTION,
        PasswordAccessLossWarningUserAction.HELP_CENTER,
        PasswordAccessLossWarningUserAction.DISMISS,
        PasswordAccessLossWarningUserAction.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordAccessLossWarningUserAction {
        int MAIN_ACTION = 0;
        int HELP_CENTER = 1;
        int DISMISS = 2;
        int COUNT = 3;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PasswordAccessLossWarningExportStep.PWD_SERIALIZATION_FAILED,
        PasswordAccessLossWarningExportStep.EXPORT_CANCELED,
        PasswordAccessLossWarningExportStep.NO_SCREEN_LOCK_SET_UP,
        PasswordAccessLossWarningExportStep.SAVE_PWD_FILE_FAILED,
        PasswordAccessLossWarningExportStep.AUTHENTICATION_EXPIRED,
        PasswordAccessLossWarningExportStep.EXPORT_DONE,
        PasswordAccessLossWarningExportStep.IMPORT_CANCELED,
        PasswordAccessLossWarningExportStep.PASSWORD_IMPORT,
        PasswordAccessLossWarningExportStep.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordAccessLossWarningExportStep {
        int PWD_SERIALIZATION_FAILED = 0;
        int EXPORT_CANCELED = 1;
        int NO_SCREEN_LOCK_SET_UP = 2;
        int SAVE_PWD_FILE_FAILED = 3;
        // Authentication can expire while Chrome app was in the background with an ongoing export
        // flow. In this case the export flow is aborted when Chrome app is foregrounded.
        int AUTHENTICATION_EXPIRED = 4;
        // This step ends the flow with no GMS Core.
        int EXPORT_DONE = 5;
        int IMPORT_CANCELED = 6;
        // This step ends the flow with new GMS Core and migration failed.
        int PASSWORD_IMPORT = 7;
        int COUNT = 8;
    }

    static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_PREFIX =
            "PasswordManager.PasswordAccessLossWarningSheet.";
    static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_SUFFIX = ".UserAction";

    static final String ACCESS_LOSS_DIALOG_METRIC_PREFIX =
            "PasswordManager.PasswordAccessLossWarningDialog";
    static final String ACCESS_LOSS_DIALOG_SHOWN_SUFFIX = ".Shown";
    static final String ACCESS_LOSS_DIALOG_ACTION_SUFFIX = ".UserAction";
    static final String EXPORT_FLOW_METRIC_TITLE =
            "PasswordManager.PasswordAccessLossWarningExportFlow.";

    static final String EXPORT_FLOW_FINAL_STEP_SUFFIX = ".FinalStep";

    static void logAccessLossWarningSheetUserAction(
            @PasswordAccessLossWarningType int warningType,
            @PasswordAccessLossWarningUserAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                getUserActionHistogramName(warningType),
                action,
                PasswordAccessLossWarningUserAction.COUNT);
    }

    private AccessLossWarningMetricsRecorder() {}

    @VisibleForTesting
    static String getUserActionHistogramName(@PasswordAccessLossWarningType int warningType) {
        // The name of the histogram will have the format:
        // PasswordManager.PasswordAccessLossWarningSheet.{AccessLossWarningType}.UserAction
        return PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_PREFIX
                + getAccessLossWarningTypeName(warningType)
                + PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_SUFFIX;
    }

    static void logDialogShownMetric(@PasswordAccessLossWarningType int warningType) {
        RecordHistogram.recordEnumeratedHistogram(
                getDialogShownHistogramName(),
                warningType,
                PasswordAccessLossWarningType.MAX_VALUE);
    }

    static void logDialogUserActionMetric(
            @PasswordAccessLossWarningType int warningType,
            @PasswordAccessLossWarningUserAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                getDialogUserActionHistogramName(warningType),
                action,
                PasswordAccessLossWarningUserAction.COUNT);
    }

    public static void logExportFlowLastStepMetric(
            @PasswordAccessLossWarningType int warningType,
            @PasswordAccessLossWarningExportStep int exportStep) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList
                        .UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)) {
            // This metric should only be logged if the password access loss warning is shown.
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                getExportFlowFinalStepHistogramName(warningType),
                exportStep,
                PasswordAccessLossWarningExportStep.COUNT);
    }

    @VisibleForTesting
    static String getDialogShownHistogramName() {
        // The name of the histogram will be PasswordManager.PasswordAccessLossWarningDialog.Shown
        return ACCESS_LOSS_DIALOG_METRIC_PREFIX + ACCESS_LOSS_DIALOG_SHOWN_SUFFIX;
    }

    @VisibleForTesting
    static String getDialogUserActionHistogramName(@PasswordAccessLossWarningType int warningType) {
        // The name of the histogram will will have the format:
        // PasswordManager.PasswordAccessLossWarningDialog.{AccessLossWarningType}.UserAction
        return ACCESS_LOSS_DIALOG_METRIC_PREFIX
                + getAccessLossWarningTypeName(warningType)
                + ACCESS_LOSS_DIALOG_ACTION_SUFFIX;
    }

    @VisibleForTesting
    public static String getExportFlowFinalStepHistogramName(
            @PasswordAccessLossWarningType int warningType) {
        // The name of the histogram will have the format:
        // PasswordManager.PasswordAccessLossWarningExportFlow.{AccessLossWarningType}.FinalStep
        return EXPORT_FLOW_METRIC_TITLE
                + getAccessLossWarningTypeName(warningType)
                + EXPORT_FLOW_FINAL_STEP_SUFFIX;
    }

    private static String getAccessLossWarningTypeName(
            @PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return "NoGmsCore";
            case PasswordAccessLossWarningType.NO_UPM:
                return "NoUPM";
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return "OnlyAccountUpm";
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return "NewGmsCoreMigrationFailed";
        }
        assert false : "Unhandled warning type: " + warningType;
        return null;
    }
}
