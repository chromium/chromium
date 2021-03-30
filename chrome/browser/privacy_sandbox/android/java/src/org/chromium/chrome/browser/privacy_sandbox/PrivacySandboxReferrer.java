// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused. To be kept in sync with PrivacySandboxReferrer in
 * enums.xml.
 */
@IntDef({PrivacySandboxReferrer.PRIVACY_SETTINGS, PrivacySandboxReferrer.COOKIES_SNACKBAR})
@Retention(RetentionPolicy.SOURCE)
public @interface PrivacySandboxReferrer {
    /**
     * Corresponds to the Settings > Privacy and security page.
     */
    int PRIVACY_SETTINGS = 0;
    /**
     * Corresponds to clicking on the snackbar on the cookies Site Settings page.
     */
    int COOKIES_SNACKBAR = 1;
    int COUNT = 2;
}
