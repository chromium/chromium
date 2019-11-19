// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * The shopping cart contents and total.
 */
public class ShoppingCart {
    private LineItem mTotal;
    @Nullable private List<LineItem> mContents;
    @Nullable private List<LineItem> mAdditionalContents;

    /**
     * Builds the shopping cart UI data model.
     *
     * @param totalPrice The total price.
     * @param contents The shopping cart contents. The breakdown of the total price. OK to be null.
     */
    public ShoppingCart(LineItem totalPrice, @Nullable List<LineItem> contents) {
        mTotal = totalPrice;
        mContents = contents;
    }

    /**
     * Returns the total price.
     *
     * @return The total price.
     */
    public LineItem getTotal() {
        return mTotal;
    }

    /**
     * Updates the total price.
     *
     * @param total The total price.
     */
    public void setTotal(LineItem total) {
        mTotal = total;
    }

    /**
     * Returns the shopping cart items, including both the original items and the additional items
     * that vary depending on the selected payment instrument, e.g., debut card discounts.
     *
     * @return The shopping cart items. Can be null. Should not be modified.
     */
    @Nullable public List<LineItem> getContents() {
        if (mContents == null && mAdditionalContents == null) return null;

        List<LineItem> result = new ArrayList<>();
        if (mContents != null) result.addAll(mContents);
        if (mAdditionalContents != null) result.addAll(mAdditionalContents);

        return Collections.unmodifiableList(result);
    }

    /**
     * Updates the shopping cart items.
     *
     * @param contents The shopping cart items. Can be null.
     */
    public void setContents(@Nullable List<LineItem> contents) {
        mContents = contents;
    }

    /**
     * Update the additional shopping cart items that vary depending on the selected payment
     * instrument, e.g., debit card discounts.
     *
     * @param additionalContents The additional shopping cart items. Can be null.
     */
    public void setAdditionalContents(@Nullable List<LineItem> additionalContents) {
        mAdditionalContents = additionalContents;
    }
}
