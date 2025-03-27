// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

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

    static void recordNoGmsNoPasswordsDialogShown() {
        RecordHistogram.recordBooleanHistogram(NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM, true);
    }

    static void recordOldGmsNoPasswordsDialogDismissalReason(boolean accepted) {
        RecordHistogram.recordBooleanHistogram(
                OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM, accepted);
    }
}
