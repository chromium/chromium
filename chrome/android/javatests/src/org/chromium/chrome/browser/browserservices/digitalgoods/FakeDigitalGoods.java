// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.PaymentCurrencyAmount;

import java.util.HashMap;
import java.util.Map;

/** A fake implementation of the {@link DigitalGoods} mojo interface for testing. */
class FakeDigitalGoods implements DigitalGoods {
    private final Map<String, ItemDetails> mItems = new HashMap<>();

    public void addItem(
            String id, String title, String description, String priceCurrency, String priceAmount) {
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
    public void getDetails(String[] itemIds, GetDetails_Response callback) {
        // Figure out the size of the results array.
        int size = 0;
        for (String id : itemIds) {
            if (mItems.containsKey(id)) size++;
        }

        ItemDetails[] result = new ItemDetails[size];
        int current = 0;
        for (String id : itemIds) {
            if (mItems.containsKey(id)) result[current++] = mItems.get(id);
        }

        callback.call(0, result);
    }

    @Override
    public void listPurchases(ListPurchases_Response callback) {}

    @Override
    public void listPurchaseHistory(ListPurchaseHistory_Response callback) {}

    @Override
    public void consume(String purchaseToken, Consume_Response callback) {}

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
