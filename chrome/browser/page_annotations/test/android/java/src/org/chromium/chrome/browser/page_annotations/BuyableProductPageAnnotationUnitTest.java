// Copyright 2021 The Chromium Authors. All rights reserved.
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
    private static final long FAKE_PRICE_MICORS = 10000L;
    private static final String FAKE_CURRENCY_CODE = "USD";

    @Test
    public void testFromValidJsonObject() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, FAKE_CURRENCY_CODE));
        Assert.assertNotNull(annotation);
        Assert.assertEquals(annotation.getType(), PageAnnotationType.BUYABLE_PRODUCT);
        Assert.assertEquals(annotation.getCurrencyCode(), FAKE_CURRENCY_CODE);
        Assert.assertEquals(annotation.getCurrentPriceMicros(), FAKE_PRICE_MICORS);
    }

    @Test
    public void testFromMissingPriceMetadataJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson());
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromMissingPriceMicrosJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, null, FAKE_CURRENCY_CODE));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromMissingCurrencyCodeJson() throws JSONException {
        BuyableProductPageAnnotation annotation = BuyableProductPageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(
                        true, FAKE_PRICE_MICORS, null));
        Assert.assertNull(annotation);
    }
}
