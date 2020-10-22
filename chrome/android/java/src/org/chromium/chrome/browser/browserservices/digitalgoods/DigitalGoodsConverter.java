// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;

/**
 * The *Converter classes take care of converting between the mojo types that
 * {@link DigitalGoodsImpl} deals with and the Android types that {@link TrustedWebActivityClient}
 * details with.
 *
 * Ideally these classes would have no Chromium dependencies that are not from Mojo (in a *.mojom.*
 * package) to allow it to be more easily reused in ARC++.
 */
public class DigitalGoodsConverter {
    private static final String TAG = "DigitalGoods";

    // These values are copied from the Play Billing library since Chrome cannot depend on it.
    // https://developer.android.com/reference/com/android/billingclient/api/BillingClient.BillingResponseCode
    static final int PLAY_BILLING_OK = 0;
    static final int PLAY_BILLING_ITEM_ALREADY_OWNED = 7;
    static final int PLAY_BILLING_ITEM_NOT_OWNED = 8;
    static final int PLAY_BILLING_ITEM_UNAVAILABLE = 4;

    private DigitalGoodsConverter() {}

    static int convertResponseCodes(int responseCode) {
        switch (responseCode) {
            case PLAY_BILLING_OK:
                return BillingResponseCode.OK;
            case PLAY_BILLING_ITEM_ALREADY_OWNED:
                return BillingResponseCode.ITEM_ALREADY_OWNED;
            case PLAY_BILLING_ITEM_NOT_OWNED:
                return BillingResponseCode.ITEM_NOT_OWNED;
            case PLAY_BILLING_ITEM_UNAVAILABLE:
                return BillingResponseCode.ITEM_UNAVAILABLE;
            default:
                Log.w(TAG, "Unexpected response code: " + responseCode);
                return BillingResponseCode.ERROR;
        }
    }
}
