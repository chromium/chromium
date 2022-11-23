// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_check;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;

/**
 * Helper class for recording password check metrics.
 */
public final class PasswordCheckMetricsRecorder {
    private PasswordCheckMetricsRecorder(){};

    public static void recordPasswordCheckReferrer(
            @PasswordCheckReferrer int passwordCheckReferrer) {
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.BulkCheck.PasswordCheckReferrerAndroid2", passwordCheckReferrer,
                PasswordCheckReferrer.COUNT);
    }

    public static void recordUiUserAction(@PasswordCheckUserAction int userAction) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.BulkCheck.UserActionAndroid",
                userAction, PasswordCheckUserAction.COUNT);
    }

    public static void recordCompromisedCredentialsCountAfterCheck(int count) {
        RecordHistogram.recordCount1000Histogram(
                "PasswordManager.BulkCheck.CompromisedCredentialsCountAfterCheckAndroid", count);
    }

    public static void recordCheckResolutionAction(
            @PasswordCheckResolutionAction int action, CompromisedCredential credential) {
        // TODO(crbug.com/1386065): Update histogram name.
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.AutomaticChange.AcceptanceWithoutAutoButton", action,
                PasswordCheckResolutionAction.COUNT);
    }
}
