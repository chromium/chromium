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
        AccountConsistencyPromoAction.STARTED_INCOGNITO_SESSION,
        AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT,
        AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
        AccountConsistencyPromoAction.SHOWN,
        AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
        AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT,
        AccountConsistencyPromoAction.DISMISSED_SCRIM,
        AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN,
        AccountConsistencyPromoAction.DISMISSED_OTHER,
        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
        AccountConsistencyPromoAction.DISMISSED_BUTTON,
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

    /**
     * User tapped the button from the expanded account list to open the incognito interstitial
     * then confirmed opening the page in the incognito tab by tapping |Continue| in the incognito
     * interstitial.
     */
    int STARTED_INCOGNITO_SESSION = 3;

    /**
     * User has selected the default account and signed in with it.
     */
    int SIGNED_IN_WITH_DEFAULT_ACCOUNT = 4;

    /**
     * User has selected one of the non default accounts and signed in with it.
     */
    int SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT = 5;

    /**
     * The promo was shown to user.
     */
    int SHOWN = 6;

    /**
     * Promo is not shown due to sign-in being disallowed either by an enterprise policy
     * or by |Allow Chrome sign-in| toggle.
     */
    int SUPPRESSED_SIGNIN_NOT_ALLOWED = 7;

    /**
     * User has added an account and signed in with this account.
     * When this metric is recorded, we won't record SIGNED_IN_WITH_DEFAULT_ACCOUNT or
     * SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT.
     */
    int SIGNED_IN_WITH_ADDED_ACCOUNT = 8;

    /**
     * User has dismissed the promo by tapping on the scrim above the bottom sheet.
     */
    int DISMISSED_SCRIM = 9;

    /**
     * User has dismissed the promo by swiping down the bottom sheet.
     */
    int DISMISSED_SWIPE_DOWN = 10;

    /**
     * User has dismissed the promo by other means.
     */
    int DISMISSED_OTHER = 11;

    /**
     * The auth error screen was shown to the user.
     */
    int AUTH_ERROR_SHOWN = 12;

    /**
     * The generic error screen was shown to the user.
     */
    int GENERIC_ERROR_SHOWN = 13;

    /**
     * User has dismissed the promo by tapping on the dismissal button in the bottom sheet.
     */
    int DISMISSED_BUTTON = 14;

    int MAX = 15;
}
