// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_ALREADY_OWNED;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_NOT_OWNED;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_ITEM_UNAVAILABLE;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_OK;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertResponseCode;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.PurchaseReference;

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

        String[] out = b.getStringArray(GetDetailsConverter.PARAM_GET_DETAILS_ITEM_IDS);
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
        assertSubsItemDetails(item, null, null, null, null, null);
    }

    @Test
    public void convertItemDetails_subscriptions() {
        String subsPeriod = "2 weeks";
        String freeTrialPeriod = "1 week";
        String introPriceCurrency = "GBP";
        String introPriceValue = "3.0";
        String introPricePeriod = "1 month";

        Bundle bundle = GetDetailsConverter.createItemDetailsBundle("id", "Title", "desc", "GBP",
                "10.0", subsPeriod, freeTrialPeriod, introPriceCurrency, introPriceValue,
                introPricePeriod);

        ItemDetails item = GetDetailsConverter.convertItemDetails(bundle);
        assertSubsItemDetails(item, subsPeriod, freeTrialPeriod, introPriceCurrency,
                introPriceValue, introPricePeriod);
    }

    /**
     * A class to allow passing values out of callbacks.
     */
    private static class TestState<T> {
        public int responseCode;
        public T[] results;
    }

    @Test
    public void convertGetDetailsCallback() {
        TestState<ItemDetails> state = new TestState<>();
        GetDetails_Response callback = (responseCode, itemDetails) -> {
            state.responseCode = responseCode;
            state.results = itemDetails;
        };

        TrustedWebActivityCallback convertedCallback =
                GetDetailsConverter.convertCallback(callback);

        int responseCode = 0;
        Bundle args = GetDetailsConverter.createResponseBundle(responseCode,
                GetDetailsConverter.createItemDetailsBundle("1", "t1", "d1", "c1", "v1"),
                GetDetailsConverter.createItemDetailsBundle(
                        "2", "t2", "d2", "c2", "v2", "sp2", "ftp2", "ipc2", "ipv2", "ipp2"));

        convertedCallback.onExtraCallback(GetDetailsConverter.RESPONSE_COMMAND, args);

        assertEquals(DigitalGoodsConverter.convertResponseCode(responseCode, Bundle.EMPTY),
                state.responseCode);
        assertItemDetails(state.results[0], "1", "t1", "d1", "c1", "v1");
        assertSubsItemDetails(state.results[0], null, null, null, null, null);
        assertItemDetails(state.results[1], "2", "t2", "d2", "c2", "v2");
        assertSubsItemDetails(state.results[1], "sp2", "ftp2", "ipc2", "ipv2", "ipp2");
    }

    private static void assertItemDetails(ItemDetails item, String id, String title, String desc,
            String currency, String value) {
        assertEquals(id, item.itemId);
        assertEquals(title, item.title);
        assertEquals(desc, item.description);
        assertEquals(currency, item.price.currency);
        assertEquals(value, item.price.value);
    }

    private static void assertSubsItemDetails(ItemDetails item, @Nullable String subsPeriod,
            @Nullable String freeTrialPeriod, @Nullable String introPriceCurrency,
            @Nullable String introPriceValue, @Nullable String intoPricePeriod) {
        assertEquals(subsPeriod, item.subscriptionPeriod);
        assertEquals(freeTrialPeriod, item.freeTrialPeriod);
        if (introPriceCurrency == null || introPriceValue == null) {
            assertNull(item.introductoryPrice);
        } else {
            assertEquals(introPriceCurrency, item.introductoryPrice.currency);
            assertEquals(introPriceValue, item.introductoryPrice.value);
        }
        assertEquals(intoPricePeriod, item.introductoryPricePeriod);
    }

    @Test
    public void convertListPurchases() {
        String id = "id";
        String token = "token";
        boolean acknowledged = true;
        int state = 2;
        long time = 1234L;
        boolean autoRenew = true;

        Bundle bundle = ListPurchasesConverter.createPurchaseDetailsBundle(
                id, token, acknowledged, state, time, autoRenew);

        PurchaseReference reference = ListPurchasesConverter.convertPurchaseDetails(bundle);
        assertPurchaseReference(reference, id, token);
    }

    private static void assertPurchaseReference(
            PurchaseReference reference, String itemId, String purchaseToken) {
        assertEquals(reference.itemId, itemId);
        assertEquals(reference.purchaseToken, purchaseToken);
    }

    @Test
    public void convertListPurchases_wrongTypes() {
        Bundle validBundle = ListPurchasesConverter.createPurchaseDetailsBundle(
                "id", "token", true, 1, 2L, true);

        assertNotNull(ListPurchasesConverter.convertPurchaseDetails(validBundle));

        {
            Bundle bundle = validBundle.deepCopy();
            bundle.putInt(ListPurchasesConverter.KEY_ITEM_ID, 5);
            assertNull(ListPurchasesConverter.convertPurchaseDetails(bundle));
        }

        {
            Bundle bundle = validBundle.deepCopy();
            bundle.putInt(ListPurchasesConverter.KEY_PURCHASE_TOKEN, 5);
            assertNull(ListPurchasesConverter.convertPurchaseDetails(bundle));
        }
    }

    @Test
    public void convertListPurchasesCallback() {
        TestState<PurchaseReference> state = new TestState<>();
        ListPurchases_Response callback = (responseCode, purchaseDetails) -> {
            state.responseCode = responseCode;
            state.results = purchaseDetails;
        };

        TrustedWebActivityCallback convertedCallback =
                ListPurchasesConverter.convertCallback(callback);

        int responseCode = 0;
        Bundle args = ListPurchasesConverter.createResponseBundle(responseCode,
                ListPurchasesConverter.createPurchaseDetailsBundle("1", "t1", true, 1, 1L, true),
                ListPurchasesConverter.createPurchaseDetailsBundle("2", "t2", false, 2, 2L, false));

        convertedCallback.onExtraCallback(ListPurchasesConverter.RESPONSE_COMMAND, args);

        assertEquals(DigitalGoodsConverter.convertResponseCode(responseCode, Bundle.EMPTY),
                state.responseCode);
        assertPurchaseReference(state.results[0], "1", "t1");
        assertPurchaseReference(state.results[1], "2", "t2");
    }

    @Test
    public void convertResponseCodes_v0() {
        Bundle args = Bundle.EMPTY;

        assertEquals(BillingResponseCode.OK,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_OK, args));
        assertEquals(BillingResponseCode.ITEM_ALREADY_OWNED,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_ITEM_ALREADY_OWNED, args));
        assertEquals(BillingResponseCode.ITEM_NOT_OWNED,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_ITEM_NOT_OWNED, args));
        assertEquals(BillingResponseCode.ITEM_UNAVAILABLE,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_ITEM_UNAVAILABLE, args));

        // Check that other numbers get set to ERROR.
        assertEquals(BillingResponseCode.ERROR, DigitalGoodsConverter.convertResponseCode(2, args));
        assertEquals(
                BillingResponseCode.ERROR, DigitalGoodsConverter.convertResponseCode(-1, args));
        assertEquals(
                BillingResponseCode.ERROR, DigitalGoodsConverter.convertResponseCode(10, args));
    }

    @Test
    public void convertResponseCodes_v1() {
        Bundle args = new Bundle();
        args.putInt(DigitalGoodsConverter.KEY_VERSION, 1);

        assertEquals(BillingResponseCode.OK, convertResponseCode(BillingResponseCode.OK, args));
        assertEquals(BillingResponseCode.ITEM_ALREADY_OWNED,
                convertResponseCode(BillingResponseCode.ITEM_ALREADY_OWNED, args));
        assertEquals(BillingResponseCode.ITEM_NOT_OWNED,
                convertResponseCode(BillingResponseCode.ITEM_NOT_OWNED, args));
        assertEquals(BillingResponseCode.ITEM_UNAVAILABLE,
                convertResponseCode(BillingResponseCode.ITEM_UNAVAILABLE, args));

        assertEquals(BillingResponseCode.ERROR, convertResponseCode(123, args));
        assertEquals(BillingResponseCode.ERROR, convertResponseCode(-12, args));
    }
}
