// Copyright 2020 The Chromium Authors. All rights reserved.
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
@IntDef({PasswordCheckReferrer.PASSWORD_SETTINGS, PasswordCheckReferrer.SAFETY_CHECK,
        PasswordCheckReferrer.LEAK_DIALOG})
@Retention(RetentionPolicy.SOURCE)
public @interface PasswordCheckReferrer {
    /**
     * Corresponds to the Settings > Passwords page.
     */
    int PASSWORD_SETTINGS = 0;
    /**
     * Corresponds to the safety check settings page.
     */
    int SAFETY_CHECK = 1;
    /**
     * Represents the leak dialog prompted to the user when they sign in with a credential
     * which was part of a data breach;
     */
    int LEAK_DIALOG = 2;
    int COUNT = 3;
}
