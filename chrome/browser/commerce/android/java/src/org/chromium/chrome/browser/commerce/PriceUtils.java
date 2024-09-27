// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.components.payments.CurrencyFormatter;

import java.util.Locale;

/** Utilities to format price strings for UI. */
public class PriceUtils {
    private static final int FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS = 2;
    private static final int FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS = 0;
    private static final int MICROS_TO_UNITS = 1000000;
    private static final long TEN_UNITS = 10 * MICROS_TO_UNITS;

    /**
     * Format the price based on shopping requirement, with an existing {@link CurrencyFormatter}.
     *
     * @param currencyFormatter The currency formatter, which already set the currency code. Must
     *     call {@link CurrencyFormatter#destroy()} afterward.
     * @param priceMicros The price in micros.
     * @return The formatted price.
     */
    public static String formatPrice(CurrencyFormatter currencyFormatter, long priceMicros) {
        // TODO(crbug.com/40720561) support all currencies
        String formattedPrice;
        // 2 fractional digits below 10 units.
        if (priceMicros < TEN_UNITS) {
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS);
            formattedPrice =
                    String.format(
                            Locale.getDefault(),
                            "%.2f",
                            (100 * priceMicros / ((double) MICROS_TO_UNITS)) / 100.0);
        } else {
            // Round up when greater than 10 units, 0 fractional digits.
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS);
            formattedPrice =
                    String.format(
                            Locale.getDefault(),
                            "%d",
                            (long)
                                    Math.floor(
                                            (double) (priceMicros + MICROS_TO_UNITS / 2)
                                                    / MICROS_TO_UNITS));
        }
        return currencyFormatter.format(formattedPrice);
    }

    /**
     * Format the price based on shopping requirement.
     * @param currencyCode The currency code. Most commonly, this follows ISO 4217 format. E.g.
     *         "USD".
     * @param priceMicros The price in micros.
     * @return The formatted price, or null if failed to format the price.
     */
    public static @Nullable String formatPrice(String currencyCode, long priceMicros) {
        if (TextUtils.isEmpty(currencyCode)) return null;
        CurrencyFormatter currencyFormatter =
                new CurrencyFormatter(currencyCode, Locale.getDefault());
        String formattedPrice = formatPrice(currencyFormatter, priceMicros);
        currencyFormatter.destroy();
        return formattedPrice;
    }
}
