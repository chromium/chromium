// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Util class to be used when recording metrics for one of the password manager dialogs shown as
 * part of the login database deprecation process.
 */
class PwmDeprecationDialogsMetricsRecorder {
    static final String NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM =
            "PasswordManager.UPM.NoGmsNoPasswordsDialogShown";

    static void recordNoGmsNoPasswordsDialogShown() {
        RecordHistogram.recordBooleanHistogram(NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM, true);
    }
}
