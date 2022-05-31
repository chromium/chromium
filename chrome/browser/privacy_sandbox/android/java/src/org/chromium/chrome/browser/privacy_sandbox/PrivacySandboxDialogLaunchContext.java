// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Indicates in which context the Privacy Sandbox dialog was launched.
 */
@IntDef({PrivacySandboxDialogLaunchContext.BROWSER_START,
        PrivacySandboxDialogLaunchContext.NEW_TAB_PAGE})
@Retention(RetentionPolicy.SOURCE)
public @interface PrivacySandboxDialogLaunchContext {
    /**
     * Corresponds to all the prompts shown on browser start.
     */
    int BROWSER_START = 0;
    /**
     * Corresponds to the bottom sheet notice on the NTP.
     */
    int NEW_TAB_PAGE = 1;
    int COUNT = 2;
}
