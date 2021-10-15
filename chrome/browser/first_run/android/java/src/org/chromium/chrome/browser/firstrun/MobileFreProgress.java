// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is used for histograms to track the FRE progress.
 * It should therefore be treated as append-only.
 * See {@code MobileFreProgress} in tools/metrics/histograms/enums.xml.
 */
@IntDef({
        MobileFreProgress.STARTED,
        MobileFreProgress.WELCOME_SHOWN,
        MobileFreProgress.DATA_SAVER_SHOWN,
        MobileFreProgress.SYNC_CONSENT_SHOWN,
        MobileFreProgress.COMPLETED_SYNC,
        MobileFreProgress.COMPLETED_NOT_SYNC,
        MobileFreProgress.DEFAULT_SEARCH_ENGINE_SHOWN,
        MobileFreProgress.WELCOME_ADD_ACCOUNT,
        MobileFreProgress.MAX,
})
@Retention(RetentionPolicy.SOURCE)
public @interface MobileFreProgress {
    int STARTED = 0;
    int WELCOME_SHOWN = 1;
    int DATA_SAVER_SHOWN = 2;
    int SYNC_CONSENT_SHOWN = 3;
    int COMPLETED_SYNC = 4;
    int COMPLETED_NOT_SYNC = 5;
    int DEFAULT_SEARCH_ENGINE_SHOWN = 6;
    /** The user started adding account from welcome screen. */
    int WELCOME_ADD_ACCOUNT = 7;
    int MAX = 8;
}
