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

    public static void recordCompromisedCredentialsCountAfterCheck(
            int countTotal, int countWithAutoChange) {
        RecordHistogram.recordCount1000Histogram(
                "PasswordManager.BulkCheck.CompromisedCredentialsCountAfterCheckAndroid",
                countTotal);
        RecordHistogram.recordCount1000Histogram("PasswordManager.BulkCheck."
                        + "CompromisedCredentialsCountWithAutoChangeAfterCheckAndroid",
                countWithAutoChange);
    }

    public static void recordCheckResolutionAction(
            @PasswordCheckResolutionAction int action, CompromisedCredential credential) {
        if (credential.hasStartableScript()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.AutomaticChange.ForSitesWithScripts", action,
                    PasswordCheckResolutionAction.COUNT);
        }
        if (credential.hasAutoChangeButton()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.AutomaticChange.AcceptanceWithAutoButton", action,
                    PasswordCheckResolutionAction.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.AutomaticChange.AcceptanceWithoutAutoButton", action,
                    PasswordCheckResolutionAction.COUNT);
        }
    }
}
