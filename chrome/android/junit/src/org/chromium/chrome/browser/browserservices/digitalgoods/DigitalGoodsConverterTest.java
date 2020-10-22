// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.browserservices.digitalgoods.AcknowledgeConverter.PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN;
import static org.chromium.chrome.browser.browserservices.digitalgoods.AcknowledgeConverter.PARAM_ACKNOWLEDGE_PURCHASE_TOKEN;
import static org.chromium.chrome.browser.browserservices.digitalgoods.AcknowledgeConverter.RESPONSE_ACKNOWLEDGE;
import static org.chromium.chrome.browser.browserservices.digitalgoods.AcknowledgeConverter.RESPONSE_ACKNOWLEDGE_RESPONSE_CODE;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_ALREADY_OWNED;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_NOT_OWNED;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_UNAVAILABLE;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_OK;
import static org.chromium.chrome.browser.browserservices.digitalgoods.GetDetailsConverter.PARAM_GET_DETAILS_ITEM_IDS;
import static org.chromium.chrome.browser.browserservices.digitalgoods.GetDetailsConverter.RESPONSE_GET_DETAILS;
import static org.chromium.chrome.browser.browserservices.digitalgoods.GetDetailsConverter.RESPONSE_GET_DETAILS_DETAILS_LIST;
import static org.chromium.chrome.browser.browserservices.digitalgoods.GetDetailsConverter.RESPONSE_GET_DETAILS_RESPONSE_CODE;

import android.os.Bundle;
import android.os.Parcelable;

import androidx.browser.trusted.TrustedWebActivityCallback;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.ItemDetails;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for {@link DigitalGoodsConverterTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DigitalGoodsConverterTest {
    // TODO(peconn): Add tests for error cases as well.

    @Test
    public void convertGetDetailsParams() {
        String[] itemIds = { "id1", "id2" };

        Bundle b = GetDetailsConverter.convertParams(itemIds);

        String[] out = b.getStringArray(PARAM_GET_DETAILS_ITEM_IDS);
        assertArrayEquals(itemIds, out);
    }

    @Test
    public void convertItemDetails() {
        String id = "id";
        String title = "Item";
        String desc = "An item.";
        String currency = "GBP";
        String value = "10";

        Bundle bundle =
                GetDetailsConverter.createItemDetailsBundle(id, title, desc, currency, value);

        ItemDetails item = GetDetailsConverter.convertItemDetails(bundle);
        assertItemDetails(item, id, title, desc, currency, value);
    }

    /**
     * A class to allow passing values out of the callback in {@link #convertGetDetailsCallback}.
     */
    private static class TestState {
        public int responseCode;
        public ItemDetails[] itemDetails;
    }

    @Test
    public void convertGetDetailsCallback() {
        TestState state = new TestState();
        GetDetailsResponse callback = (responseCode, itemDetails) -> {
            state.responseCode = responseCode;
            state.itemDetails = itemDetails;
        };

        TrustedWebActivityCallback convertedCallback =
                GetDetailsConverter.convertCallback(callback);

        Bundle args = new Bundle();
        int responseCode = 0;
        Parcelable[] items = {
                GetDetailsConverter.createItemDetailsBundle("1", "t1", "d1", "c1", "v1"),
                GetDetailsConverter.createItemDetailsBundle("2", "t2", "d2", "c2", "v2")};
        args.putInt(RESPONSE_GET_DETAILS_RESPONSE_CODE, responseCode);
        args.putParcelableArray(RESPONSE_GET_DETAILS_DETAILS_LIST, items);

        convertedCallback.onExtraCallback(RESPONSE_GET_DETAILS, args);

        assertEquals(responseCode, state.responseCode);
        assertItemDetails(state.itemDetails[0], "1", "t1", "d1", "c1", "v1");
        assertItemDetails(state.itemDetails[1], "2", "t2", "d2", "c2", "v2");
    }

    private static void assertItemDetails(ItemDetails item, String id, String title, String desc,
            String currency, String value) {
        assertEquals(id, item.itemId);
        assertEquals(title, item.title);
        assertEquals(desc, item.description);
        assertEquals(currency, item.price.currency);
        assertEquals(value, item.price.value);
    }

    @Test
    public void convertAcknowledgeParams() {
        String token = "abcdef";
        boolean makeAvailableAgain = true;

        Bundle b = AcknowledgeConverter.convertParams(token, makeAvailableAgain);

        String outToken = b.getString(PARAM_ACKNOWLEDGE_PURCHASE_TOKEN);
        boolean outMakeAvailableAgain = b.getBoolean(PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN);

        assertEquals(token, outToken);
        assertEquals(makeAvailableAgain, outMakeAvailableAgain);
    }

    @Test
    public void convertAcknowledgeCallback() {
        // Since there's only one value we want to get out of the callback, we can use Atomic*
        // instead of creating a new class.
        AtomicInteger state = new AtomicInteger();

        AcknowledgeResponse callback = (responseCode) -> state.set(responseCode);

        TrustedWebActivityCallback convertedCallback =
                AcknowledgeConverter.convertCallback(callback);

        Bundle args = new Bundle();
        int responseCode = 0;
        args.putInt(RESPONSE_ACKNOWLEDGE_RESPONSE_CODE, responseCode);

        convertedCallback.onExtraCallback(RESPONSE_ACKNOWLEDGE, args);

        assertEquals(responseCode, state.get());
    }

    @Test
    public void convertResponseCodes() {
        assertEquals(BillingResponseCode.OK,
                DigitalGoodsConverter.convertResponseCodes(PLAY_BILLING_OK));
        assertEquals(BillingResponseCode.ITEM_ALREADY_OWNED,
                DigitalGoodsConverter.convertResponseCodes(PLAY_BILLING_ITEM_ALREADY_OWNED));
        assertEquals(BillingResponseCode.ITEM_NOT_OWNED,
                DigitalGoodsConverter.convertResponseCodes(PLAY_BILLING_ITEM_NOT_OWNED));
        assertEquals(BillingResponseCode.ITEM_UNAVAILABLE,
                DigitalGoodsConverter.convertResponseCodes(PLAY_BILLING_ITEM_UNAVAILABLE));

        // Check that other numbers get set to ERROR.
        assertEquals(BillingResponseCode.ERROR,
                DigitalGoodsConverter.convertResponseCodes(2));
        assertEquals(BillingResponseCode.ERROR,
                DigitalGoodsConverter.convertResponseCodes(-1));
        assertEquals(BillingResponseCode.ERROR,
                DigitalGoodsConverter.convertResponseCodes(10));
    }
}