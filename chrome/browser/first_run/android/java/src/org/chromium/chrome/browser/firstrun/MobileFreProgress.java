// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is used for histograms to track the FRE progress. It should therefore be treated as
 * append-only. See {@code MobileFreProgress} in tools/metrics/histograms/enums.xml.
 */
@IntDef({
    MobileFreProgress.STARTED,
    MobileFreProgress.WELCOME_SHOWN,
    MobileFreProgress.DEPRECATED_DATA_SAVER_SHOWN,
    MobileFreProgress.SYNC_CONSENT_SHOWN,
    MobileFreProgress.SYNC_CONSENT_ACCEPTED,
    MobileFreProgress.SYNC_CONSENT_DISMISSED,
    MobileFreProgress.DEFAULT_SEARCH_ENGINE_SHOWN,
    MobileFreProgress.WELCOME_ADD_ACCOUNT,
    MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT,
    MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT,
    MobileFreProgress.WELCOME_DISMISS,
    MobileFreProgress.SYNC_CONSENT_SETTINGS_LINK_CLICK,
    MobileFreProgress.HISTORY_SYNC_OPT_IN_SHOWN,
    MobileFreProgress.HISTORY_SYNC_ACCEPTED,
    MobileFreProgress.HISTORY_SYNC_DISMISSED,
    MobileFreProgress.MAX,
})
@Retention(RetentionPolicy.SOURCE)
public @interface MobileFreProgress {
    int STARTED = 0;
    int WELCOME_SHOWN = 1;
    int DEPRECATED_DATA_SAVER_SHOWN = 2;
    int SYNC_CONSENT_SHOWN = 3;

    /** The user clicked on the continue button to continue with sync consent. */
    int SYNC_CONSENT_ACCEPTED = 4;

    /** The user clicked on the |No thanks| button to continue without sync consent. */
    int SYNC_CONSENT_DISMISSED = 5;

    int DEFAULT_SEARCH_ENGINE_SHOWN = 6;

    /** The user started adding account from welcome screen. */
    int WELCOME_ADD_ACCOUNT = 7;

    /** The user signed in with default account from welcome screen. */
    int WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT = 8;

    /** The user signed in with non-default account from welcome screen. */
    int WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT = 9;

    /** The user clicked the dismiss button on welcome screen. */
    int WELCOME_DISMISS = 10;

    /** The user clicked on the |settings| link on sync consent screen. */
    int SYNC_CONSENT_SETTINGS_LINK_CLICK = 11;

    int HISTORY_SYNC_OPT_IN_SHOWN = 12;

    /** The user clicked on the |Yes, I'm in| button to accept history sync. */
    int HISTORY_SYNC_ACCEPTED = 13;

    /** The user clicked on the |No thanks| button to decline history sync. */
    int HISTORY_SYNC_DISMISSED = 14;

    int MAX = 15;
}
