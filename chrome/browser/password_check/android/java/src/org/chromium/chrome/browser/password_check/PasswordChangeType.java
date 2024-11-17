// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused. To be kept in sync with PasswordChangeType in
 * enums.xml.
 */
@IntDef({PasswordChangeType.UNKNOWN, PasswordChangeType.MANUAL_CHANGE})
@Retention(RetentionPolicy.SOURCE)
public @interface PasswordChangeType {
    int UNKNOWN = 0;

    /** A user opened a site to change a password manually. */
    int MANUAL_CHANGE = 1;

    // Deprecated as a part of APC removal (crbug.com/1386065).
    // int AUTOMATED_CHANGE = 2;
}
