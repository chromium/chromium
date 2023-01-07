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
 * Tests for {@link ProductPriceUpdatePageAnnotation}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ProductPriceUpdatePageAnnotationUnitTest {
    private static final String FAKE_OLD_PRICE_MICORS = "10000";
    private static final String FAKE_NEW_PRICE_MICORS = "10";
    private static final String FAKE_CURRENCY_CODE = "USD";

    @Test
    public void testFromValidJsonObject() throws JSONException {
        ProductPriceUpdatePageAnnotation annotation = ProductPriceUpdatePageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeProductPriceUpdate(FAKE_OLD_PRICE_MICORS,
                        FAKE_CURRENCY_CODE, FAKE_NEW_PRICE_MICORS, FAKE_CURRENCY_CODE));

        Assert.assertNotNull(annotation);
        Assert.assertEquals(PageAnnotationType.PRODUCT_PRICE_UPDATE, annotation.getType());
        Assert.assertEquals(10000L, annotation.getOldPriceMicros());
        Assert.assertEquals(FAKE_CURRENCY_CODE, annotation.getCurrencyCode());

        Assert.assertEquals(10L, annotation.getNewPriceMicros());
        Assert.assertEquals(FAKE_CURRENCY_CODE, annotation.getCurrencyCode());
    }

    @Test
    public void testFromMissingPriceMetadataJson() throws JSONException {
        ProductPriceUpdatePageAnnotation annotation = ProductPriceUpdatePageAnnotation.fromJson(
                PageAnnotationsTestUtils.createEmptyProductPriceUpdate());
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromJsonMissingNewPrice() throws JSONException {
        ProductPriceUpdatePageAnnotation annotation = ProductPriceUpdatePageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeProductPriceUpdate(
                        FAKE_OLD_PRICE_MICORS, FAKE_CURRENCY_CODE, null, null));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromJsonMissingOldPrice() throws JSONException {
        ProductPriceUpdatePageAnnotation annotation = ProductPriceUpdatePageAnnotation.fromJson(
                PageAnnotationsTestUtils.createFakeProductPriceUpdate(
                        null, null, FAKE_NEW_PRICE_MICORS, FAKE_CURRENCY_CODE));
        Assert.assertNull(annotation);
    }

    @Test
    public void testFromJsonBadAmountMicros() throws JSONException {
        ProductPriceUpdatePageAnnotation badNewPriceAnnotation =
                ProductPriceUpdatePageAnnotation.fromJson(
                        PageAnnotationsTestUtils.createFakeProductPriceUpdate(FAKE_OLD_PRICE_MICORS,
                                FAKE_CURRENCY_CODE, "NOT_A_LONG", FAKE_CURRENCY_CODE));
        Assert.assertNull(badNewPriceAnnotation);

        ProductPriceUpdatePageAnnotation badOldPriceAnnotation =
                ProductPriceUpdatePageAnnotation.fromJson(
                        PageAnnotationsTestUtils.createFakeProductPriceUpdate(
                                "", FAKE_CURRENCY_CODE, FAKE_NEW_PRICE_MICORS, FAKE_CURRENCY_CODE));
        Assert.assertNull(badOldPriceAnnotation);
    }
}
