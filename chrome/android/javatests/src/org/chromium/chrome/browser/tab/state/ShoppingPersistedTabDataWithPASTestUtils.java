// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.annotation.IntDef;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.page_annotations.BuyableProductPageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsService;
import org.chromium.chrome.browser.page_annotations.ProductPriceUpdatePageAnnotation;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
/**
 * Helper class for {@link ShoppingPersistedTabDataWithPASTest} & {@link
 * ShoppingPersistedTabDataLegacyWithPASTest}.
 */
public abstract class ShoppingPersistedTabDataWithPASTestUtils {
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
    static final String UNITED_STATES_CURRENCY_CODE = "USD";
    static final String FAKE_OFFER_ID = "100";

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
                                        PRICE_MICROS, UNITED_STATES_CURRENCY_CODE, FAKE_OFFER_ID));
                                break;
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_PRICE_UPDATED:
                                add(new BuyableProductPageAnnotation(UPDATED_PRICE_MICROS,
                                        UNITED_STATES_CURRENCY_CODE, FAKE_OFFER_ID));
                                break;
                            case MockPageAnnotationsResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE:
                                add(new BuyableProductPageAnnotation(
                                        PRICE_MICROS, UNITED_STATES_CURRENCY_CODE, FAKE_OFFER_ID));
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

    static void verifyGetPageAnnotationsCalled(
            PageAnnotationsService pageAnnotationsService, int numTimes) {
        verify(pageAnnotationsService, times(numTimes))
                .getAnnotations(any(GURL.class), any(Callback.class));
    }
}