// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.annotation.IntDef;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.page_annotations.BuyableProductPageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsService;
import org.chromium.chrome.browser.page_annotations.ProductPriceUpdatePageAnnotation;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicReference;
/**
 * Helper class for {@link ShoppingPersistedTabDataTest} & {@link
 * ShoppingPersistedTabDataLegacyTest}.
 */
public abstract class ShoppingPersistedTabDataTestUtils {
    @IntDef({MockPageAnnotationsResponse.BUYABLE_PRODUCT_INITIAL,
            MockPageAnnotationsResponse.BUYABLE_PRODUCT_PRICE_UPDATED,
            MockPageAnnotationsResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE,
            MockPageAnnotationsResponse.PRODUCT_PRICE_UPDATE,
            MockPageAnnotationsResponse.BUYABLE_PRODUCT_EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    @interface MockPageAnnotationsResponse {
        int BUYABLE_PRODUCT_INITIAL = 0;
        int BUYABLE_PRODUCT_PRICE_UPDATED = 1;
        int BUYABLE_PRODUCT_AND_PRODUCT_UPDATE = 2;
        int PRODUCT_PRICE_UPDATE = 3;
        int BUYABLE_PRODUCT_EMPTY = 4;
    }

    static final long PRICE_MICROS = 123456789012345L;
    static final long UPDATED_PRICE_MICROS = 287000000L;
    static final long HIGH_PRICE_MICROS = 141000000L;
    static final long LOW_PRICE_MICROS = 100000000L;
    static final String HIGH_PRICE_FORMATTED = "$141";
    static final String LOW_PRICE_FORMATTED = "$100";
    static final String UNITED_STATES_CURRENCY_CODE = "USD";
    static final String GREAT_BRITAIN_CURRENCY_CODE = "GBP";
    static final String JAPAN_CURRENCY_CODE = "JPY";
    static final int TAB_ID = 1;
    static final boolean IS_INCOGNITO = false;

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithDefaults() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));
        shoppingPersistedTabData.setCurrencyCode(UNITED_STATES_CURRENCY_CODE);
        return shoppingPersistedTabData;
    }

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithCurrencyCode(
            int tabId, boolean isIncognito, String currencyCode) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(tabId, isIncognito));
        shoppingPersistedTabData.setCurrencyCode(currencyCode);
        return shoppingPersistedTabData;
    }

    static Tab createTabOnUiThread(int tabId, boolean isIncognito) {
        AtomicReference<Tab> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = MockTab.createAndInitialize(TAB_ID, IS_INCOGNITO);
            CriticalPersistedTabData.from(tab).setTimestampMillis(System.currentTimeMillis());
            res.set(tab);
        });
        return res.get();
    }

    static long getTimeLastUpdatedOnUiThread(Tab tab) {
        AtomicReference<Long> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            res.set(PersistedTabData.from(tab, ShoppingPersistedTabData.class)
                            .getLastPriceChangeTimeMs());
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

    static void mockOptimizationGuideResponse(OptimizationGuideBridge.Natives optimizationGuideJni,
            @OptimizationGuideDecision int decision) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                callback.onOptimizationGuideDecision(decision, null);
                return null;
            }
        })
                .when(optimizationGuideJni)
                .canApplyOptimization(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }

    static void mockPageAnnotationsResponse(PageAnnotationsService pageAnnotationsService,
            @MockPageAnnotationsResponse int expectedResponse) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(new LinkedList<PageAnnotation>() {
                    {
                        switch (expectedResponse) {
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_INITIAL:
                                add(new BuyableProductPageAnnotation(
                                        PRICE_MICROS, UNITED_STATES_CURRENCY_CODE));
                                break;
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_PRICE_UPDATED:
                                add(new BuyableProductPageAnnotation(
                                        UPDATED_PRICE_MICROS, UNITED_STATES_CURRENCY_CODE));
                                break;
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE:
                                add(new BuyableProductPageAnnotation(
                                        PRICE_MICROS, UNITED_STATES_CURRENCY_CODE));
                                add(new ProductPriceUpdatePageAnnotation(PRICE_MICROS,
                                        UPDATED_PRICE_MICROS, UNITED_STATES_CURRENCY_CODE));
                                break;
                            case MockPageAnnotationsResponse.PRODUCT_PRICE_UPDATE:
                                add(new ProductPriceUpdatePageAnnotation(PRICE_MICROS,
                                        UPDATED_PRICE_MICROS, UNITED_STATES_CURRENCY_CODE));
                                break;
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_EMPTY:
                            default:
                                break;
                        }
                    }
                });
                return null;
            }
        })
                .when(pageAnnotationsService)
                .getAnnotations(any(GURL.class), any(Callback.class));
    }

    static void verifyEndpointFetcherCalled(EndpointFetcher.Natives endpointFetcher, int numTimes) {
        verify(endpointFetcher, times(numTimes))
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), any(Callback.class));
    }

    static void verifyGetPageAnnotationsCalled(
            PageAnnotationsService pageAnnotationsService, int numTimes) {
        verify(pageAnnotationsService, times(numTimes))
                .getAnnotations(any(GURL.class), any(Callback.class));
    }
}