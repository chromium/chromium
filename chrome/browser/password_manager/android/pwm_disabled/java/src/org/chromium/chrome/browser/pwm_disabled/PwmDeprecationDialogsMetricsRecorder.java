// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/**
 * Util class to be used when recording metrics for one of the password manager dialogs shown as
 * part of the login database deprecation process.
 */
@NullMarked
class PwmDeprecationDialogsMetricsRecorder {
    static final String NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM =
            "PasswordManager.UPM.NoGmsNoPasswordsDialogShown";

    static final String OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM =
            "PasswordManager.UPM.OldGmsNoPasswordsDialogDismissalReason";

    static final String DOWNLOAD_CSV_FLOW_LAST_STEP_HISTOGRAM_SUFFIX = "DownloadCsvFlowLastStep";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Keep in sync with DownloadCsvFlowSteps in the passwords-specific enums.xml file.
    @IntDef({
        DownloadCsvFlowStep.DISMISSED_DIALOG,
        DownloadCsvFlowStep.NO_SCREEN_LOCK,
        DownloadCsvFlowStep.REAUTH_FAILED,
        DownloadCsvFlowStep.CANCELLED_FILE_SELECTION,
        DownloadCsvFlowStep.CANT_FIND_SOURCE_CSV,
        DownloadCsvFlowStep.CSV_WRITE_FAILED,
        DownloadCsvFlowStep.SUCCESS
    })
    @interface DownloadCsvFlowStep {
        int DISMISSED_DIALOG = 0;
        int NO_SCREEN_LOCK = 1;
        int REAUTH_FAILED = 2;
        int CANCELLED_FILE_SELECTION = 3;
        int CANT_FIND_SOURCE_CSV = 4;
        int CSV_WRITE_FAILED = 5;
        int SUCCESS = 6;
        int COUNT = 7;
    }

    @IntDef({
        DownloadCsvDialogType.NO_GMS,
        DownloadCsvDialogType.OLD_GMS,
        DownloadCsvDialogType.FULL_UPM_SUPPORT_GMS
    })
    @interface DownloadCsvDialogType {
        int NO_GMS = 0;
        int OLD_GMS = 1;
        int FULL_UPM_SUPPORT_GMS = 2;
    }

    static void recordNoGmsNoPasswordsDialogShown() {
        RecordHistogram.recordBooleanHistogram(NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM, true);
    }

    static void recordOldGmsNoPasswordsDialogDismissalReason(boolean accepted) {
        RecordHistogram.recordBooleanHistogram(
                OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM, accepted);
    }

    static void recordLastStepOfDownloadCsvFlow(
            @DownloadCsvDialogType int dialogType, @DownloadCsvFlowStep int lastStep) {
        String dialogTypeInfix = null;
        switch (dialogType) {
            case DownloadCsvDialogType.NO_GMS:
                {
                    dialogTypeInfix = "NoGms";
                    break;
                }
            case DownloadCsvDialogType.OLD_GMS:
                {
                    dialogTypeInfix = "OldGms";
                    break;
                }
            case DownloadCsvDialogType.FULL_UPM_SUPPORT_GMS:
                {
                    dialogTypeInfix = "FullUpmSupportGms";
                    break;
                }
            default:
                assert false : "Wrong dialog type: " + dialogType;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.UPM."
                        + dialogTypeInfix
                        + "."
                        + DOWNLOAD_CSV_FLOW_LAST_STEP_HISTOGRAM_SUFFIX,
                lastStep,
                DownloadCsvFlowStep.COUNT);
    }
}
