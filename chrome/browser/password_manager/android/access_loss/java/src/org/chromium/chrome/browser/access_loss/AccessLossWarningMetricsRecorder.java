// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

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

    static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_PREFIX =
            "PasswordManager.PasswordAccessLossWarningSheet.";
    static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_SUFFIX = ".UserAction";

    static final String ACCESS_LOSS_DIALOG_METRIC_PREFIX =
            "PasswordManager.PasswordAccessLossWarningDialog";
    static final String ACCESS_LOSS_DIALOG_SHOWN_SUFFIX = ".Shown";
    static final String ACCESS_LOSS_DIALOG_ACTION_SUFFIX = ".UserAction";

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

    @VisibleForTesting
    static String getDialogShownHistogramName() {
        // The name of the histogram will be PasswordManager.PasswordAccessLossWarningDialog.Shown
        return ACCESS_LOSS_DIALOG_METRIC_PREFIX + ACCESS_LOSS_DIALOG_SHOWN_SUFFIX;
    }

    @VisibleForTesting
    static String getDialogUserActionHistogramName(@PasswordAccessLossWarningType int warningType) {
        // The name of the histogram will will have the format:
        // PasswordManager.PasswordAccessLossWarningDialog.{AccessLossWarningType}.UserAction"
        return ACCESS_LOSS_DIALOG_METRIC_PREFIX
                + getAccessLossWarningTypeName(warningType)
                + ACCESS_LOSS_DIALOG_ACTION_SUFFIX;
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
