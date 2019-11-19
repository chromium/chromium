// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;

/** Place to define and control payment preferences. */
public class PaymentPreferencesUtil {
    // Avoid instantiation by accident.
    private PaymentPreferencesUtil() {}

    /** Preference to indicate whether payment request has been completed successfully once.*/
    private static final String PAYMENT_COMPLETE_ONCE = "payment_complete_once";

    /** Prefix of the preferences to persist use count of the payment instruments. */
    public static final String PAYMENT_INSTRUMENT_USE_COUNT_ = "payment_instrument_use_count_";

    /** Prefix of the preferences to persist last use date of the payment instruments. */
    public static final String PAYMENT_INSTRUMENT_USE_DATE_ = "payment_instrument_use_date_";

    /**
     * Checks whehter the payment request has been successfully completed once.
     *
     * @return True If payment request has been successfully completed once.
     */
    public static boolean isPaymentCompleteOnce() {
        return ContextUtils.getAppSharedPreferences().getBoolean(PAYMENT_COMPLETE_ONCE, false);
    }

    /** Sets the payment request has been successfully completed once. */
    public static void setPaymentCompleteOnce() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PAYMENT_COMPLETE_ONCE, true)
                .apply();
    }

    /**
     * Gets use count of the payment instrument.
     *
     * @param id The instrument identifier.
     * @return The use count.
     */
    public static int getPaymentInstrumentUseCount(String id) {
        return ContextUtils.getAppSharedPreferences().getInt(PAYMENT_INSTRUMENT_USE_COUNT_ + id, 0);
    }

    /**
     * Increase use count of the payment instrument by one.
     *
     * @param id The instrument identifier.
     */
    public static void increasePaymentInstrumentUseCount(String id) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PAYMENT_INSTRUMENT_USE_COUNT_ + id, getPaymentInstrumentUseCount(id) + 1)
                .apply();
    }

    /**
     * A convenient method to set use count of the payment instrument to a specific value for test.
     *
     * @param id    The instrument identifier.
     * @param count The count value.
     */
    @VisibleForTesting
    public static void setPaymentInstrumentUseCountForTest(String id, int count) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PAYMENT_INSTRUMENT_USE_COUNT_ + id, count)
                .apply();
    }

    /**
     * Gets last use date of the payment instrument.
     *
     * @param id The instrument identifier.
     * @return The time difference between the last use date and 'midnight, January 1, 1970 UTC' in
     *         millieseconds.
     */
    public static long getPaymentInstrumentLastUseDate(String id) {
        return ContextUtils.getAppSharedPreferences().getLong(PAYMENT_INSTRUMENT_USE_DATE_ + id, 0);
    }

    /**
     * Sets last use date of the payment instrument.
     *
     * @param id   The instrument identifier.
     * @param date The time difference between the last use date and 'midnight, January 1, 1970 UTC'
     *             in millieseconds.
     */
    public static void setPaymentInstrumentLastUseDate(String id, long date) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(PAYMENT_INSTRUMENT_USE_DATE_ + id, date)
                .apply();
    }
}
