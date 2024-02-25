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
    PasswordCheckResolutionAction.OPENED_SITE,
    PasswordCheckResolutionAction.DELETED_PASSWORD,
    PasswordCheckResolutionAction.EDITED_PASSWORD,
    PasswordCheckResolutionAction.DID_NOTHING
})
@Retention(RetentionPolicy.SOURCE)
public @interface PasswordCheckResolutionAction {
    /** A user opened a site to change a password manually. */
    int OPENED_SITE = 0;

    /** Deprecated as a part of APC removal (crbug.com/1386065). int STARTED_SCRIPT = 1; */

    /** A user deleted a password. */
    int DELETED_PASSWORD = 2;

    /** A user edited a password. */
    int EDITED_PASSWORD = 3;

    /** A user did nothing. */
    int DID_NOTHING = 4;

    int COUNT = 5;
}
