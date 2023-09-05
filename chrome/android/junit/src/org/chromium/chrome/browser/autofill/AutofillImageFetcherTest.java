// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.image_fetcher.test.TestImageFetcher;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link AutofillImageFetcher}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillImageFetcherTest {
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    private AutofillImageFetcher mAutofillImageFetcher;

    @Before
    public void setUp() {
        mAutofillImageFetcher = new AutofillImageFetcher(new TestImageFetcher(TEST_CARD_ART_IMAGE));
    }

    @After
    public void tearDown() {
        mAutofillImageFetcher.clearCachedImagesForTesting();
    }

    @Test
    @SmallTest
    public void testFetchAndCacheImages_validUrl_successfulImagefetch() {
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");
        mAutofillImageFetcher.prefetchImages(new GURL[] {validUrl});

        Map<String, Bitmap> expectedImagesCache = new HashMap<>();
        expectedImagesCache.put(validUrl.getSpec(), TEST_CARD_ART_IMAGE);

        Assert.assertEquals(mAutofillImageFetcher.getCachedImagesForTesting(), expectedImagesCache);
    }

    @Test
    @SmallTest
    public void testFetchAndCacheImages_validUrl_unsuccessfulImagefetch() {
        mAutofillImageFetcher = new AutofillImageFetcher(new TestImageFetcher(null));

        GURL validUrl = new GURL("https://www.google.com/valid-image-url");
        mAutofillImageFetcher.prefetchImages(new GURL[] {validUrl});

        Assert.assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testFetchAndCacheImages_emptyOrInvalidUrl() {
        GURL emptyUrl = new GURL("");
        GURL invalidUrl = new GURL("invalid-image-url");
        mAutofillImageFetcher.prefetchImages(new GURL[] {emptyUrl, invalidUrl});

        Assert.assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testFetchAndCacheImages_duplicateValidUrl_imageFetchedOnlyOnce() {
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");
        mAutofillImageFetcher.prefetchImages(new GURL[] {validUrl, validUrl});

        // Since the second URL is a duplicate, expect only 1 cached image.
        Map<String, Bitmap> expectedImagesCache = new HashMap<>();
        expectedImagesCache.put(validUrl.getSpec(), TEST_CARD_ART_IMAGE);

        Assert.assertEquals(mAutofillImageFetcher.getCachedImagesForTesting(), expectedImagesCache);
    }
}
