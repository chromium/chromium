// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.commerce.PriceUtils;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of the improved bookmark row. */
class ShoppingAccessoryViewProperties {
    /** Encapsulates price info on a product. */
    public static class PriceInfo {
        private final long mOriginalPrice;
        private final long mCurrentPrice;
        private final CurrencyFormatter mFormatter;

        /**
         * @param originalPrice The original price.
         * @param currentPrice The current price.
         * @param formatter A formatter used to translate the given long to a string to display.
         */
        public PriceInfo(long originalPrice, long currentPrice, CurrencyFormatter formatter) {
            mOriginalPrice = originalPrice;
            mCurrentPrice = currentPrice;
            mFormatter = formatter;
        }

        /** Returns whether there's a price drop. */
        public boolean isPriceDrop() {
            return mOriginalPrice > mCurrentPrice;
        }

        /** Returns the text to display for the original price. */
        public String getOriginalPriceText() {
            return PriceUtils.formatPrice(mFormatter, mOriginalPrice);
        }

        /** Returns the text to display for the current price. */
        public String getCurrentPriceText() {
            return PriceUtils.formatPrice(mFormatter, mCurrentPrice);
        }
    }

    static final WritableObjectPropertyKey<PriceInfo> PRICE_INFO =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey PRICE_TRACKED = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {PRICE_INFO, PRICE_TRACKED};
}
