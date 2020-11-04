// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.DigitalGoods.ListPurchasesResponse;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.PaymentCurrencyAmount;

import java.util.HashMap;
import java.util.Map;

/**
 * A fake implementation of the {@link DigitalGoods} mojo interface for testing.
 */
class FakeDigitalGoods implements DigitalGoods {
    private final Map<String, ItemDetails> mItems = new HashMap<>();

    public void addItem(String id, String title, String description, String priceCurrency,
            String priceAmount) {
        PaymentCurrencyAmount amount = new PaymentCurrencyAmount();
        amount.currency = priceCurrency;
        amount.value = priceAmount;

        ItemDetails item = new ItemDetails();
        item.itemId = id;
        item.title = title;
        item.description = description;
        item.price = amount;

        mItems.put(id, item);
    }

    @Override
    public void getDetails(String[] itemIds, GetDetailsResponse callback) {
        // Figure out the size of the results array.
        int size = 0;
        for (String id: itemIds) {
            if (mItems.containsKey(id)) size++;
        }

        ItemDetails[] result = new ItemDetails[size];
        int current = 0;
        for (String id: itemIds) {
            if (mItems.containsKey(id)) result[current++] = mItems.get(id);
        }

        callback.call(0, result);
    }

    @Override
    public void acknowledge(
            String purchaseToken, boolean makeAvailableAgain, AcknowledgeResponse callback) {}

    @Override
    public void listPurchases(ListPurchasesResponse callback) {}

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
