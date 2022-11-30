// Copyright 2020 The Chromium Authors
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

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link PageAnnotationUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageAnnotationUtilsUnitTest {
    private static final List<PageAnnotation> DUMMY_ANNOTATIONS_LIST = Arrays.asList(
            new PageAnnotation[] {new BuyableProductPageAnnotation(100L, "USD", "200")});

    private static class DummyPageAnnotation extends PageAnnotation {
        DummyPageAnnotation() {
            super(PageAnnotationType.UNKNOWN);
        }
    }

    @Test
    public void testCreateFromJsonUnknownType() throws JSONException {
        PageAnnotation annotation = PageAnnotationUtils.createPageAnnotationFromJson(
                PageAnnotationsTestUtils.createDummyPageAnnotationJson("DUMMY_TYPE"));
        Assert.assertNull(annotation);
    }

    @Test
    public void testCreateFromJsonBuyableProduct() throws JSONException {
        PageAnnotation annotation = PageAnnotationUtils.createPageAnnotationFromJson(
                PageAnnotationsTestUtils.createFakeBuyableProductJson(true, "100", "USD", "200"));
        Assert.assertNotNull(annotation);
    }

    @Test
    public void testCreateProductPriceUpdate() throws JSONException {
        PageAnnotation annotation = PageAnnotationUtils.createPageAnnotationFromJson(
                PageAnnotationsTestUtils.createFakeProductPriceUpdate("100", "USD", "10", "USD"));
        Assert.assertNotNull(annotation);
        Assert.assertEquals(annotation.getType(), PageAnnotationType.PRODUCT_PRICE_UPDATE);
    }

    @Test
    public void testGetAnnotationByType() {
        BuyableProductPageAnnotation annotation = PageAnnotationUtils.getAnnotation(
                DUMMY_ANNOTATIONS_LIST, BuyableProductPageAnnotation.class);
        Assert.assertNotNull(annotation);
    }

    @Test
    public void testGetAnnotationByTypeInvalid() {
        DummyPageAnnotation annotation = PageAnnotationUtils.getAnnotation(
                DUMMY_ANNOTATIONS_LIST, DummyPageAnnotation.class);
        Assert.assertNull(annotation);
    }
}
