// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class is used to record user action that was taken after receiving the header
 * from Gaia in the web sign-in flow.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
@IntDef({
        AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS,
        AccountConsistencyPromoAction.DISMISSED_BACK,
        AccountConsistencyPromoAction.ADD_ACCOUNT,
})
@Retention(RetentionPolicy.SOURCE)
public @interface AccountConsistencyPromoAction {
    /**
     * Promo is not shown as there are no accounts on device.
     */
    int SUPPRESSED_NO_ACCOUNTS = 0;

    /**
     * User has dismissed the promo by tapping back button.
     */
    int DISMISSED_BACK = 1;

    /**
     * User has tapped |Add account to device| from expanded account list.
     */
    int ADD_ACCOUNT = 2;

    int MAX = 3;
}
