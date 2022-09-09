// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.R;
import org.chromium.payments.mojom.PaymentShippingType;

/** Container for custom shipping strings. */
public class ShippingStrings {
    private final int mSummaryLabel;
    private final int mAddressLabel;
    private final int mOptionLabel;
    private final int mSelectPrompt;
    private final int mUnsupported;

    /**
     * Determines the strings to be used for the given shipping type.
     *
     * @param shippingType Shipping type. One of SHIPPING, DELIVERY, and PICKUP.
     */
    public ShippingStrings(int shippingType) {
        switch (shippingType) {
            case PaymentShippingType.SHIPPING:
                mSummaryLabel = R.string.payments_shipping_summary_label;
                mAddressLabel = R.string.payments_shipping_address_label;
                mOptionLabel = R.string.payments_shipping_option_label;
                mSelectPrompt = R.string.payments_select_shipping_address_for_shipping_methods;
                mUnsupported = R.string.payments_unsupported_shipping_address;
                break;

            case PaymentShippingType.DELIVERY:
                mSummaryLabel = R.string.payments_delivery_summary_label;
                mAddressLabel = R.string.payments_delivery_address_label;
                mOptionLabel = R.string.payments_delivery_option_label;
                mSelectPrompt = R.string.payments_select_delivery_address_for_delivery_methods;
                mUnsupported = R.string.payments_unsupported_delivery_address;
                break;

            case PaymentShippingType.PICKUP:
                mSummaryLabel = R.string.payments_pickup_summary_label;
                mAddressLabel = R.string.payments_pickup_address_label;
                mOptionLabel = R.string.payments_pickup_option_label;
                mSelectPrompt = R.string.payments_select_pickup_address_for_pickup_methods;
                mUnsupported = R.string.payments_unsupported_pickup_address;
                break;

            default:
                assert false;
                mSummaryLabel = 0;
                mAddressLabel = 0;
                mOptionLabel = 0;
                mSelectPrompt = 0;
                mUnsupported = 0;
                break;
        }
    }

    /** @return The string resource for the label of shipping summary section. */
    public int getSummaryLabel() {
        return mSummaryLabel;
    }

    /** @return The string resource for the label of shipping address section. */
    public int getAddressLabel() {
        return mAddressLabel;
    }

    /** @return The string resource for the label of shipping option section. */
    public int getOptionLabel() {
        return mOptionLabel;
    }

    /** @return The string resource for the prompt to choose a shipping address. */
    public int getSelectPrompt() {
        return mSelectPrompt;
    }

    /** @return The string resource for the unsupported shipping address message. */
    public int getUnsupported() {
        return mUnsupported;
    }
}
