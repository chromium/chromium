// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.page_annotations.PageAnnotation.PageAnnotationType;

/**
 * Tests for {@link BuyableProductPageAnnotation}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BuyableProductPageAnnotationUnitTest {
    private static final String FAKE_PRICE_MICORS = "10000";
    private static final String FAKE_CURRENCY_CODE = "USD";
    private static final String FAKE_OFFER_ID = "200";

    @Test
    public void testFromValidJsonObject() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, FAKE_CURRENCY_CODE, FAKE_OFFER_ID));
        Assert.assertNotNull(annotation);
        Assert.assertEquals(PageAnnotationType.BUYABLE_PRODUCT, annotation.getType());
        Assert.assertEquals(FAKE_CURRENCY_CODE, annotation.getCurrencyCode());
        Assert.assertEquals(10000L, annotation.getCurrentPriceMicros());
    }

    @Test
    public void testFromMissingPriceMetadataJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createEmptyBuyableProduct());
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromMissingPriceMicrosJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, null, FAKE_CURRENCY_CODE, FAKE_OFFER_ID));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromMissingCurrencyCodeJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, null, FAKE_OFFER_ID));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromBadPriceAmountJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, null, FAKE_OFFER_ID));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromMissingOfferIdJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, null, null));
        Assert.assertNull(annotation);
    }
}
