// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import static org.chromium.chrome.browser.browserservices.digitalgoods.ConsumeConverter.PARAM_CONSUME_PURCHASE_TOKEN;
import static org.chromium.chrome.browser.browserservices.digitalgoods.ConsumeConverter.RESPONSE_CONSUME;
import static org.chromium.chrome.browser.browserservices.digitalgoods.ConsumeConverter.RESPONSE_CONSUME_RESPONSE_CODE;
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
import org.chromium.payments.mojom.DigitalGoods.Consume_Response;
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchaseHistory_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.ItemType;
import org.chromium.payments.mojom.PurchaseReference;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link DigitalGoodsConverterTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DigitalGoodsConverterTest {
    // TODO(peconn): Add tests for error cases as well.

    @Test
    public void convertGetDetailsParams() {
        String[] itemIds = {"id1", "id2"};

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
        assertOptionalItemDetails(item, null, null, null, null, null, null, null, 0);
    }

    @Test
    public void convertItemDetails_optional() {
        String iconUrl = "https://www.example.com/image.png";
        String subsPeriod = "2 weeks";
        String freeTrialPeriod = "1 week";
        String introPriceCurrency = "GBP";
        String introPriceValue = "3.0";
        String introPricePeriod = "1 month";
        int introPriceCycles = 4;

        Bundle bundle =
                GetDetailsConverter.createItemDetailsBundle(
                        "id",
                        "Title",
                        "desc",
                        "GBP",
                        "10.0",
                        "subs",
                        iconUrl,
                        subsPeriod,
                        freeTrialPeriod,
                        introPriceCurrency,
                        introPriceValue,
                        introPricePeriod,
                        introPriceCycles);

        ItemDetails item = GetDetailsConverter.convertItemDetails(bundle);
        assertOptionalItemDetails(
                item,
                "subs",
                iconUrl,
                subsPeriod,
                freeTrialPeriod,
                introPriceCurrency,
                introPriceValue,
                introPricePeriod,
                introPriceCycles);
    }

    /** A class to allow passing values out of callbacks. */
    private static class TestState<T> {
        public int responseCode;
        public T[] results;
    }

    @Test
    public void convertGetDetailsCallback() {
        TestState<ItemDetails> state = new TestState<>();
        GetDetails_Response callback =
                (responseCode, itemDetails) -> {
                    state.responseCode = responseCode;
                    state.results = itemDetails;
                };

        TrustedWebActivityCallback convertedCallback =
                GetDetailsConverter.convertCallback(callback);

        String iconUrl = "https://www.example.com/image.png";
        int responseCode = 0;
        Bundle args =
                GetDetailsConverter.createResponseBundle(
                        responseCode,
                        GetDetailsConverter.createItemDetailsBundle("1", "t1", "d1", "c1", "v1"),
                        GetDetailsConverter.createItemDetailsBundle(
                                "2", "t2", "d2", "c2", "v2", "inapp", iconUrl, "sp2", "ftp2",
                                "ipc2", "ipv2", "ipp2", 4));

        convertedCallback.onExtraCallback(GetDetailsConverter.RESPONSE_COMMAND, args);

        assertEquals(
                DigitalGoodsConverter.convertResponseCode(responseCode, Bundle.EMPTY),
                state.responseCode);
        assertItemDetails(state.results[0], "1", "t1", "d1", "c1", "v1");
        assertOptionalItemDetails(state.results[0], null, null, null, null, null, null, null, 0);
        assertItemDetails(state.results[1], "2", "t2", "d2", "c2", "v2");
        assertOptionalItemDetails(
                state.results[1], "inapp", iconUrl, "sp2", "ftp2", "ipc2", "ipv2", "ipp2", 4);
    }

    private static void assertItemDetails(
            ItemDetails item, String id, String title, String desc, String currency, String value) {
        assertEquals(id, item.itemId);
        assertEquals(title, item.title);
        assertEquals(desc, item.description);
        assertEquals(currency, item.price.currency);
        assertEquals(value, item.price.value);
    }

    private static void assertOptionalItemDetails(
            ItemDetails item,
            @Nullable String type,
            @Nullable String iconUrl,
            @Nullable String subsPeriod,
            @Nullable String freeTrialPeriod,
            @Nullable String introPriceCurrency,
            @Nullable String introPriceValue,
            @Nullable String intoPricePeriod,
            int introPriceCycles) {
        if (type == null) {
            assertEquals(ItemType.UNKNOWN, item.type);
        } else if ("subs".equals(type)) {
            assertEquals(ItemType.SUBSCRIPTION, item.type);
        } else if ("inapp".equals(type)) {
            assertEquals(ItemType.PRODUCT, item.type);
        } else {
            fail("Invalid item type");
        }

        if (iconUrl == null) {
            assertEquals(0, item.iconUrls.length);
        } else {
            assertEquals(1, item.iconUrls.length);
            assertEquals(iconUrl, item.iconUrls[0].url);
        }

        assertEquals(subsPeriod, item.subscriptionPeriod);
        assertEquals(freeTrialPeriod, item.freeTrialPeriod);
        if (introPriceCurrency == null || introPriceValue == null) {
            assertNull(item.introductoryPrice);
        } else {
            assertEquals(introPriceCurrency, item.introductoryPrice.currency);
            assertEquals(introPriceValue, item.introductoryPrice.value);
        }
        assertEquals(intoPricePeriod, item.introductoryPricePeriod);
        assertEquals(introPriceCycles, item.introductoryPriceCycles);
    }

    @Test
    public void convertPurchaseReference() {
        String id = "id";
        String token = "token";

        Bundle bundle = ListPurchasesConverter.createPurchaseReferenceBundle(id, token);

        PurchaseReference reference = ListPurchasesConverter.convertPurchaseReference(bundle);
        assertPurchaseReference(reference, id, token);
    }

    private static void assertPurchaseReference(
            PurchaseReference reference, String itemId, String purchaseToken) {
        assertEquals(reference.itemId, itemId);
        assertEquals(reference.purchaseToken, purchaseToken);
    }

    @Test
    public void convertListPurchases_wrongTypes() {
        Bundle validBundle = ListPurchasesConverter.createPurchaseReferenceBundle("id", "token");

        assertNotNull(ListPurchasesConverter.convertPurchaseReference(validBundle));

        {
            Bundle bundle = validBundle.deepCopy();
            bundle.putInt(ListPurchasesConverter.KEY_ITEM_ID, 5);
            assertNull(ListPurchasesConverter.convertPurchaseReference(bundle));
        }

        {
            Bundle bundle = validBundle.deepCopy();
            bundle.putInt(ListPurchasesConverter.KEY_PURCHASE_TOKEN, 5);
            assertNull(ListPurchasesConverter.convertPurchaseReference(bundle));
        }
    }

    @Test
    public void convertListPurchasesCallback() {
        TestState<PurchaseReference> state = new TestState<>();
        ListPurchases_Response callback =
                (responseCode, purchaseDetails) -> {
                    state.responseCode = responseCode;
                    state.results = purchaseDetails;
                };

        TrustedWebActivityCallback convertedCallback =
                ListPurchasesConverter.convertCallback(callback);

        int responseCode = 0;
        Bundle args =
                ListPurchasesConverter.createResponseBundle(
                        responseCode,
                        ListPurchasesConverter.createPurchaseReferenceBundle("1", "t1"),
                        ListPurchasesConverter.createPurchaseReferenceBundle("2", "t2"));

        convertedCallback.onExtraCallback(ListPurchasesConverter.RESPONSE_COMMAND, args);

        assertEquals(
                DigitalGoodsConverter.convertResponseCode(responseCode, Bundle.EMPTY),
                state.responseCode);
        assertPurchaseReference(state.results[0], "1", "t1");
        assertPurchaseReference(state.results[1], "2", "t2");
    }

    @Test
    public void convertListPurchaseHistoryCallback() {
        TestState<PurchaseReference> state = new TestState<>();
        ListPurchaseHistory_Response callback =
                (responseCode, purchaseDetails) -> {
                    state.responseCode = responseCode;
                    state.results = purchaseDetails;
                };

        TrustedWebActivityCallback convertedCallback =
                ListPurchaseHistoryConverter.convertCallback(callback);

        int responseCode = 0;
        Bundle args =
                ListPurchaseHistoryConverter.createResponseBundle(
                        responseCode,
                        ListPurchasesConverter.createPurchaseReferenceBundle("1", "t1"),
                        ListPurchasesConverter.createPurchaseReferenceBundle("2", "t2"));

        convertedCallback.onExtraCallback(ListPurchaseHistoryConverter.RESPONSE_COMMAND, args);

        assertEquals(
                DigitalGoodsConverter.convertResponseCode(responseCode, Bundle.EMPTY),
                state.responseCode);
        assertPurchaseReference(state.results[0], "1", "t1");
        assertPurchaseReference(state.results[1], "2", "t2");
    }

    @Test
    public void convertConsumeParams() {
        String token = "abcdef";

        Bundle b = ConsumeConverter.convertParams(token);

        String outToken = b.getString(PARAM_CONSUME_PURCHASE_TOKEN);

        assertEquals(token, outToken);
    }

    @Test
    public void convertConsumeCallback() {
        // Since there's only one value we want to get out of the callback, we can use Atomic*
        // instead of creating a new class.
        AtomicInteger state = new AtomicInteger();

        Consume_Response callback = (responseCode) -> state.set(responseCode);

        TrustedWebActivityCallback convertedCallback = ConsumeConverter.convertCallback(callback);

        Bundle args = new Bundle();
        int responseCode = 0;
        args.putInt(RESPONSE_CONSUME_RESPONSE_CODE, responseCode);

        convertedCallback.onExtraCallback(RESPONSE_CONSUME, args);

        assertEquals(responseCode, state.get());
    }

    @Test
    public void convertResponseCodes_v0() {
        Bundle args = Bundle.EMPTY;

        assertEquals(
                BillingResponseCode.OK,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_OK, args));
        assertEquals(
                BillingResponseCode.ITEM_ALREADY_OWNED,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_ITEM_ALREADY_OWNED, args));
        assertEquals(
                BillingResponseCode.ITEM_NOT_OWNED,
                DigitalGoodsConverter.convertResponseCode(PLAY_BILLING_ITEM_NOT_OWNED, args));
        assertEquals(
                BillingResponseCode.ITEM_UNAVAILABLE,
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
        assertEquals(
                BillingResponseCode.ITEM_ALREADY_OWNED,
                convertResponseCode(BillingResponseCode.ITEM_ALREADY_OWNED, args));
        assertEquals(
                BillingResponseCode.ITEM_NOT_OWNED,
                convertResponseCode(BillingResponseCode.ITEM_NOT_OWNED, args));
        assertEquals(
                BillingResponseCode.ITEM_UNAVAILABLE,
                convertResponseCode(BillingResponseCode.ITEM_UNAVAILABLE, args));

        assertEquals(BillingResponseCode.ERROR, convertResponseCode(123, args));
        assertEquals(BillingResponseCode.ERROR, convertResponseCode(-12, args));
    }
}
