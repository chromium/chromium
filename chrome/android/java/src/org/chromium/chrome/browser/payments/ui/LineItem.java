// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

/** The line item on the bill. */
public class LineItem {
    private final String mLabel;
    private final String mCurrency;
    private final String mPrice;
    private final boolean mIsPending;

    /**
     * Builds a line item.
     *
     * @param label     The line item label.
     * @param currency  The currency code.
     * @param price     The price string.
     * @param isPending Whether the price is pending.
     */
    public LineItem(String label, String currency, String price, boolean isPending) {
        mLabel = label;
        mCurrency = currency;
        mPrice = price;
        mIsPending = isPending;
    }

    /**
     * Returns the label of this line item. For example, “CA state tax”.
     *
     * @return The label of this line item.
     */
    public String getLabel() {
        return mLabel;
    }

    /**
     * Returns the currency code string of this line item. For example, “USD”.
     *
     * @return The currency code string of this line item.
     */
    public String getCurrency() {
        return mCurrency;
    }

    /**
     * Returns the price string of this line item. For example, “$10.00”.
     *
     * @return The price string of this line item.
     */
    public String getPrice() {
        return mPrice;
    }

    /** @return Whether the price is pending. */
    public boolean getIsPending() {
        return mIsPending;
    }
}
