// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.os.Bundle;
import android.os.Parcelable;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;

import java.util.ArrayList;
import java.util.List;

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

    static final String KEY_VERSION = "digitalgoods.version";

    // These values are copied from the Play Billing library since Chrome cannot depend on it.
    // https://developer.android.com/reference/com/android/billingclient/api/BillingClient.BillingResponseCode
    static final int PLAY_BILLING_OK = 0;
    static final int PLAY_BILLING_ITEM_ALREADY_OWNED = 7;
    static final int PLAY_BILLING_ITEM_NOT_OWNED = 8;
    static final int PLAY_BILLING_ITEM_UNAVAILABLE = 4;

    private DigitalGoodsConverter() {}

    /** Converts the given response code to one suitable for mojo. */
    static int convertResponseCode(int responseCode, Bundle bundle) {
        // In the initial development, the TWA shell provided a Play Billing response code, so it
        // needs to be converted to a Mojo one. Later on (but still before the feature was publicly
        // launched), we decided that the TWA shell should provide data to Chrome already converted
        // to a Mojo format. This is because the TWA shell may not be using Play Billing, so it
        // doesn't make sense to standardise on that.

        // We kept support for the older version just to make testing and development easier. It may
        // be removed once the feature has launched.
        int version = bundle.getInt(KEY_VERSION);
        if (version == 0) {
            return playBillingToMojoResponseCode(responseCode);
        }

        if (BillingResponseCode.isKnownValue(responseCode)) {
            return responseCode;
        }

        Log.w(TAG, "Unexpected response code: " + responseCode);
        return BillingResponseCode.ERROR;
    }

    static int playBillingToMojoResponseCode(int responseCode) {
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

    /** Checks that the given field exists and is of the required type in a Bundle. */
    static <T> boolean checkField(Bundle bundle, String key, Class<T> clazz) {
        if (bundle.containsKey(key) && clazz.isAssignableFrom(bundle.get(key).getClass())) {
            return true;
        }
        Log.w(TAG, "Missing field " + key + " of type " + clazz.getName() + ".");
        return false;
    }

    /** An interface for use with {@link #convertParcelableArray}. */
    interface Converter<T> {
        @Nullable
        T convert(Bundle bundle);
    }

    /**
     * Runs through the given {@link Parcelable[]} and for each item that is a {@link Bundle}, calls
     * {@link Converter#convert}, returning an array of the non-null results.
     */
    static <T> List<T> convertParcelableArray(Parcelable[] array, Converter<T> converter) {
        List<T> list = new ArrayList<>();
        for (Parcelable item : array) {
            if (!(item instanceof Bundle)) {
                Log.w(TAG, "Passed a Parcelable that was not a Bundle.");
                continue;
            }

            T converted = converter.convert((Bundle) item);
            if (converted != null) list.add(converted);
        }

        return list;
    }
}
