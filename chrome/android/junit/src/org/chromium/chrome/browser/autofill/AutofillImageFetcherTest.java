// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.image_fetcher.test.TestImageFetcher;
import org.chromium.url.GURL;

import java.util.Map;

/** Unit tests for {@link AutofillImageFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
@Features.DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_SERVER_SIDE_STRETCHING)
public class AutofillImageFetcherTest {
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    private AutofillImageFetcher mImageFetcher;

    @Before
    public void setUp() {
        mImageFetcher = new AutofillImageFetcher(new TestImageFetcher(TEST_CARD_ART_IMAGE));
    }

    @After
    public void tearDown() {
        mImageFetcher.clearCachedImagesForTesting();
    }

    @Test
    @SmallTest
    public void testPrefetchImages_validUrl_successfulImagefetch() {
        GURL validUrl1 = new GURL("https://www.google.com/valid-image-url-1");
        GURL validUrl2 = new GURL("https://www.google.com/valid-image-url-2");
        CardIconSpecs cardIconSpecsSmall =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.SMALL);
        CardIconSpecs cardIconSpecsLarge =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.LARGE);
        GURL cachedValidUrlSmall1 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl1, cardIconSpecsSmall.getWidth(), cardIconSpecsSmall.getHeight());
        GURL cachedValidUrlLarge1 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl1, cardIconSpecsLarge.getWidth(), cardIconSpecsLarge.getHeight());
        GURL cachedValidUrlSmall2 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl2, cardIconSpecsSmall.getWidth(), cardIconSpecsSmall.getHeight());
        GURL cachedValidUrlLarge2 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl2, cardIconSpecsLarge.getWidth(), cardIconSpecsLarge.getHeight());
        Bitmap treatedImageSmall =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecsSmall, true);
        Bitmap treatedImageLarge =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecsLarge, true);

        mImageFetcher.prefetchImages(
                new GURL[] {validUrl1, validUrl2}, new int[] {ImageSize.SMALL, ImageSize.LARGE});
        Map<String, Bitmap> cachedImages = mImageFetcher.getCachedImagesForTesting();

        // Each card art image is cached at 2 resolutions: 32x20 for the Keyboard Accessory, and
        // 40x24 on all other surfaces.
        assertEquals(cachedImages.size(), 4);
        assertTrue(treatedImageSmall.sameAs(cachedImages.get(cachedValidUrlSmall1.getSpec())));
        assertTrue(treatedImageSmall.sameAs(cachedImages.get(cachedValidUrlSmall2.getSpec())));
        assertTrue(treatedImageLarge.sameAs(cachedImages.get(cachedValidUrlLarge1.getSpec())));
        assertTrue(treatedImageLarge.sameAs(cachedImages.get(cachedValidUrlLarge2.getSpec())));
    }

    @Test
    @SmallTest
    public void testPrefetchImages_validUrl_unsuccessfulImagefetch() {
        mImageFetcher = new AutofillImageFetcher(new TestImageFetcher(null));
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");

        mImageFetcher.prefetchImages(new GURL[] {validUrl}, new int[] {ImageSize.SMALL});

        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testPrefetchImages_invalidOrEmptyUrl() {
        GURL invalidUrl = new GURL("invalid-image-url");
        GURL emptyUrl = new GURL("");

        mImageFetcher.prefetchImages(
                new GURL[] {invalidUrl, emptyUrl}, new int[] {ImageSize.SMALL});

        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testGetImagesIfAvailable() {
        GURL cardArtUrl = new GURL("https://www.google.com/card-art-url");
        CardIconSpecs cardIconSpecs =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.LARGE);
        Bitmap treatedImage =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecs, true);

        // The card art image is not present in the image cache, so a call is made to fetch the
        // image, and nothing is returned.
        assertFalse(mImageFetcher.getImageIfAvailable(cardArtUrl, cardIconSpecs).isPresent());

        // The card art image is fetched and cached from the previous call, so the cached image is
        // returned.
        assertTrue(
                mImageFetcher
                        .getImageIfAvailable(cardArtUrl, cardIconSpecs)
                        .get()
                        .sameAs(treatedImage));
    }
}
