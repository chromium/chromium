// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class should contain helpers for recording Password Manager metrics.
 */
public class PasswordMetricsUtil {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({PasswordMigrationWarningUserActions.GOT_IT,
            PasswordMigrationWarningUserActions.MORE_OPTIONS,
            PasswordMigrationWarningUserActions.SYNC, PasswordMigrationWarningUserActions.EXPORT,
            PasswordMigrationWarningUserActions.CANCEL,
            PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION,
            PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS,
            PasswordMigrationWarningUserActions.DISMISS_EMPTY_SHEET,
            PasswordMigrationWarningUserActions.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordMigrationWarningUserActions {
        int GOT_IT = 0;
        int MORE_OPTIONS = 1;
        int SYNC = 2;
        int EXPORT = 3;
        int CANCEL = 4;
        int DISMISS_INTRODUCTION = 5;
        int DISMISS_MORE_OPTIONS = 6;
        int DISMISS_EMPTY_SHEET = 7;
        int COUNT = 8;
    }
    public static final String PASSWORD_MIGRATION_WARNING_USER_ACTIONS =
            "PasswordManager.PasswordMigrationWarning.UserAction";

    /**
     * This is a helper that logs that the user has taken on the password migration warning sheet.
     * @param result is the value to be recorded
     */
    public static void logPasswordMigrationWarningUserAction(
            @PasswordMigrationWarningUserActions int result) {
        RecordHistogram.recordEnumeratedHistogram(PASSWORD_MIGRATION_WARNING_USER_ACTIONS, result,
                PasswordMigrationWarningUserActions.COUNT);
    }
}
