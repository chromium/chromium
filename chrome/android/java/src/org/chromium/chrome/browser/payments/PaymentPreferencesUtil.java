// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Place to define and control payment preferences. */
public class PaymentPreferencesUtil {
    // Avoid instantiation by accident.
    private PaymentPreferencesUtil() {}

    /**
     * Checks whehter the payment request has been successfully completed once.
     *
     * @return True If payment request has been successfully completed once.
     */
    public static boolean isPaymentCompleteOnce() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.PAYMENTS_PAYMENT_COMPLETE_ONCE, false);
    }

    /** Sets the payment request has been successfully completed once. */
    public static void setPaymentCompleteOnce() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.PAYMENTS_PAYMENT_COMPLETE_ONCE, true);
    }

    /**
     * Gets use count of the payment app.
     *
     * @param id The app identifier.
     * @return The use count.
     */
    public static int getPaymentAppUseCount(String id) {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id));
    }

    /**
     * Increase use count of the payment app by one.
     *
     * @param id The app identifier.
     */
    public static void increasePaymentAppUseCount(String id) {
        ChromeSharedPreferences.getInstance()
                .incrementInt(
                        ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id));
    }

    /**
     * A convenient method to set use count of the payment app to a specific value for test.
     *
     * @param id    The app identifier.
     * @param count The count value.
     */
    public static void setPaymentAppUseCountForTest(String id, int count) {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id),
                        count);
    }

    /**
     * Gets last use date of the payment app.
     *
     * @param id The app identifier.
     * @return The time difference between the last use date and 'midnight, January 1, 1970 UTC' in
     *         milliseconds.
     */
    public static long getPaymentAppLastUseDate(String id) {
        return ChromeSharedPreferences.getInstance()
                .readLong(ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE.createKey(id));
    }

    /**
     * Sets last use date of the payment app.
     *
     * @param id   The app identifier.
     * @param date The time difference between the last use date and 'midnight, January 1, 1970 UTC'
     *             in milliseconds.
     */
    public static void setPaymentAppLastUseDate(String id, long date) {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE.createKey(id),
                        date);
    }
}
