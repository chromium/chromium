// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.annotation.IntDef;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helper class for {@link ShoppingPersistedTabDataTest} & {@link
 * ShoppingPersistedTabDataLegacyTest}.
 */
public abstract class ShoppingPersistedTabDataTestUtils {
    @IntDef({
        ShoppingServiceResponse.NONE,
        ShoppingServiceResponse.PRICE,
        ShoppingServiceResponse.PRICE_DROP_1,
        ShoppingServiceResponse.PRICE_DROP_2,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ShoppingServiceResponse {
        /** Price only (no price drop) */
        int PRICE = 0;

        /***
         * Price drop 1 (new price = UPDATED_PRICE_MICROS, previous price = PRICE_MICROS).
         */
        int PRICE_DROP_1 = 1;

        /***
         * Price drop 2 (new price = LOW_PRICE_MICROS, previous price = HIGH_PRICE_MICROS).
         */
        int PRICE_DROP_2 = 2;

        /***
         * No price or price drop.
         */
        int NONE = 3;
    }

    static final GURL DEFAULT_GURL = new GURL("https://www.google.com");
    static final GURL GURL_FOO = new GURL("https://www.foo.com");
    static final GURL GURL_BAR = new GURL("https://www.bar.com");
    static final long PRICE_MICROS = 123456789012345L;
    static final long UPDATED_PRICE_MICROS = 287000000L;
    static final long HIGH_PRICE_MICROS = 141000000L;
    static final long LOW_PRICE_MICROS = 100000000L;
    static final String HIGH_PRICE_FORMATTED = "$141";
    static final String LOW_PRICE_FORMATTED = "$100";
    static final String COUNTRY_CODE = "US";
    static final String UNITED_STATES_CURRENCY_CODE = "USD";
    static final String GREAT_BRITAIN_CURRENCY_CODE = "GBP";
    static final String JAPAN_CURRENCY_CODE = "JPY";
    static final int TAB_ID = 1;
    static final boolean IS_INCOGNITO = false;
    static final String FAKE_OFFER_ID = "100";
    static final String FAKE_PRODUCT_TITLE = "Product Title";
    static final String FAKE_PRODUCT_TITLE_TWO = "Product Title Two";
    static final String FAKE_PRODUCT_IMAGE_URL = "https://www.google.com/image";
    static final String FAKE_PRODUCT_IMAGE_URL_TWO = "https://www.google.com/image_2";

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithDefaults(Profile profile) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, profile));
        shoppingPersistedTabData.setCurrencyCode(UNITED_STATES_CURRENCY_CODE);
        shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
        return shoppingPersistedTabData;
    }

    static ShoppingPersistedTabData createSavedShoppingPersistedTabDataOnUiThread(Tab tab) {
        AtomicReference<ShoppingPersistedTabData> res = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData shoppingPersistedTabData =
                            new ShoppingPersistedTabData(tab);
                    ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
                    supplier.set(true);
                    shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
                    shoppingPersistedTabData.enableSaving();
                    shoppingPersistedTabData.setPriceMicros(PRICE_MICROS);
                    shoppingPersistedTabData.setPreviousPriceMicros(UPDATED_PRICE_MICROS);
                    shoppingPersistedTabData.setLastUpdatedMs(System.currentTimeMillis());
                    shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
                    shoppingPersistedTabData.save();
                    res.set(shoppingPersistedTabData);
                });
        return res.get();
    }

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithCurrencyCode(
            int tabId, Profile profile, String currencyCode) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(tabId, profile));
        shoppingPersistedTabData.setCurrencyCode(currencyCode);
        shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
        return shoppingPersistedTabData;
    }

    static MockTab createTabOnUiThread(int tabId, Profile profile) {
        AtomicReference<MockTab> res = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MockTab tab = MockTab.createAndInitialize(tabId, profile);
                    tab.setIsInitialized(true);
                    tab.setGurlOverrideForTesting(DEFAULT_GURL);
                    tab.setTimestampMillis(System.currentTimeMillis());
                    res.set(tab);
                });
        return res.get();
    }

    static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            // Throw Runtime exception to make catching InterruptedException unnecessary
            throw new RuntimeException(e);
        }
    }

    static void mockShoppingServiceResponse(
            ShoppingService shoppingService,
            GURL url,
            @ShoppingServiceResponse int expectedResponse) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                ProductInfoCallback callback =
                                        (ProductInfoCallback) invocation.getArguments()[1];
                                switch (expectedResponse) {
                                    case ShoppingServiceResponse.PRICE:
                                        callback.onResult(
                                                url,
                                                new ProductInfo(
                                                        FAKE_PRODUCT_TITLE,
                                                        new GURL(FAKE_PRODUCT_IMAGE_URL),
                                                        Optional.empty(),
                                                        Optional.of(Long.parseLong(FAKE_OFFER_ID)),
                                                        UNITED_STATES_CURRENCY_CODE,
                                                        PRICE_MICROS,
                                                        COUNTRY_CODE,
                                                        Optional.empty()));
                                        break;
                                    case ShoppingServiceResponse.PRICE_DROP_1:
                                        callback.onResult(
                                                url,
                                                new ProductInfo(
                                                        FAKE_PRODUCT_TITLE,
                                                        new GURL(FAKE_PRODUCT_IMAGE_URL),
                                                        Optional.empty(),
                                                        Optional.of(Long.parseLong(FAKE_OFFER_ID)),
                                                        UNITED_STATES_CURRENCY_CODE,
                                                        UPDATED_PRICE_MICROS,
                                                        COUNTRY_CODE,
                                                        Optional.of(PRICE_MICROS)));
                                        break;
                                    case ShoppingServiceResponse.PRICE_DROP_2:
                                        callback.onResult(
                                                url,
                                                new ProductInfo(
                                                        FAKE_PRODUCT_TITLE_TWO,
                                                        new GURL(FAKE_PRODUCT_IMAGE_URL_TWO),
                                                        Optional.empty(),
                                                        Optional.of(Long.parseLong(FAKE_OFFER_ID)),
                                                        UNITED_STATES_CURRENCY_CODE,
                                                        LOW_PRICE_MICROS,
                                                        COUNTRY_CODE,
                                                        Optional.of(HIGH_PRICE_MICROS)));
                                        break;
                                    case ShoppingServiceResponse.NONE:
                                        callback.onResult(url, null);
                                        break;
                                    default:
                                        break;
                                }
                                return null;
                            }
                        })
                .when(shoppingService)
                .getProductInfoForUrl(eq(url), any(ProductInfoCallback.class));
    }

    static void verifyShoppingServiceCalled(ShoppingService shoppingService, int numTimes) {
        verify(shoppingService, times(numTimes))
                .getProductInfoForUrl(any(GURL.class), any(ProductInfoCallback.class));
    }

    static void verifyShoppingServiceCalledWithURL(
            ShoppingService shoppingService, GURL url, int numTimes) {
        verify(shoppingService, times(numTimes))
                .getProductInfoForUrl(eq(url), any(ProductInfoCallback.class));
    }
}
