// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_check;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused. To be kept in sync with PasswordCheckReferrerAndroid in
 * enums.xml.
 */
@IntDef({
    PasswordCheckUserAction.START_CHECK_AUTOMATICALLY,
    PasswordCheckUserAction.START_CHECK_MANUALLY,
    PasswordCheckUserAction.CANCEL_CHECK,
    PasswordCheckUserAction.CHANGE_PASSWORD,
    PasswordCheckUserAction.CHANGE_PASSWORD_MANUALLY,
    PasswordCheckUserAction.VIEW_PASSWORD_CLICK,
    PasswordCheckUserAction.VIEWED_PASSWORD,
    PasswordCheckUserAction.COPIED_PASSWORD,
    PasswordCheckUserAction.EDIT_PASSWORD_CLICK,
    PasswordCheckUserAction.EDITED_PASSWORD,
    PasswordCheckUserAction.DELETE_PASSWORD_CLICK,
    PasswordCheckUserAction.DELETED_PASSWORD
})
@Retention(RetentionPolicy.SOURCE)
public @interface PasswordCheckUserAction {
    /**
     * A check was automatically started when the user entered the password check
     * view.
     */
    int START_CHECK_AUTOMATICALLY = 0;

    /** A check was started from the UI header button. */
    int START_CHECK_MANUALLY = 1;

    /** The running check was cancelled, by pressing the back button. */
    int CANCEL_CHECK = 2;

    /** The change password button was pressed. */
    int CHANGE_PASSWORD = 3;

    /**
     * Deprecated as a part of APC removal (crbug.com/1386065).
     * int CHANGE_PASSWORD_AUTOMATICALLY = 4;
     */

    /**
     * The manual change password button was pressed. This is displayed together with the
     * automatic password change button.
     */
    int CHANGE_PASSWORD_MANUALLY = 5;

    /** The user clicked the option to view a compromised password. */
    int VIEW_PASSWORD_CLICK = 6;

    /** The user viewed a compromised password. */
    int VIEWED_PASSWORD = 7;

    /** The user copied a compromised password. */
    int COPIED_PASSWORD = 8;

    /** The user clicked the option to edit a compromised password. */
    int EDIT_PASSWORD_CLICK = 9;

    /** The user edited a compromised password. */
    int EDITED_PASSWORD = 10;

    /** The user clicked the option to delete a compromised password. */
    int DELETE_PASSWORD_CLICK = 11;

    /** Password deletion was confirmed. */
    int DELETED_PASSWORD = 12;

    int COUNT = 13;
}
