// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcher.Params;
import org.chromium.url.GURL;

import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

// TODO(crbug.com/388217006): Add tests for {@link AutofillImageFetcher#getImageIfAvailable(GURL,
// CardIconSpecs)} after refactor.
/** Unit tests for {@link AutofillImageFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class AutofillImageFetcherTest {
    private static final GURL TEST_IMAGE_URL = new GURL("https://www.google.com/test-image-url");
    private static final Bitmap TEST_IMAGE = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ImageFetcher mMockImageFetcher;

    private AutofillImageFetcher mAutofillImageFetcher;
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() {
        mAutofillImageFetcher = new AutofillImageFetcher(mMockImageFetcher);
        mShadowLooper = Shadows.shadowOf(new Handler().getLooper());
        doAnswer(
                        invocation -> {
                            Params params = invocation.getArgument(0);
                            Callback callback = invocation.getArgument(1);
                            if (!params.url.contains(TEST_IMAGE_URL.getSpec())) {
                                callback.onResult(null);
                                return null;
                            }
                            callback.onResult(TEST_IMAGE);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(Params.class), any(Callback.class));
    }

    @After
    public void tearDown() {
        mAutofillImageFetcher.clearCachedImagesForTesting();
        verifyNoMoreInteractions(mMockImageFetcher);
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_validUrl_successfulImageFetch() {
        CardIconSpecs cardIconSpecsSmall =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.SMALL);
        GURL imageCacheKeySmall =
                AutofillUiUtils.getFifeIconUrlWithParams(
                        TEST_IMAGE_URL,
                        cardIconSpecsSmall.getWidth(),
                        cardIconSpecsSmall.getHeight());
        Bitmap treatedImageSmall =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_IMAGE, cardIconSpecsSmall, true);
        CardIconSpecs cardIconSpecsLarge =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.LARGE);
        GURL imageCacheKeyLarge =
                AutofillUiUtils.getFifeIconUrlWithParams(
                        TEST_IMAGE_URL,
                        cardIconSpecsLarge.getWidth(),
                        cardIconSpecsLarge.getHeight());
        Bitmap treatedImageLarge =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_IMAGE, cardIconSpecsLarge, true);
        // Both generic and credit card art image fetcher histograms should log success twice (once
        // for each size).
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ true, /* times= */ 2)
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.CreditCardArt.Result",
                                /* value= */ true,
                                /* times= */ 2)
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {TEST_IMAGE_URL}, new int[] {ImageSize.SMALL, ImageSize.LARGE});
        Map<String, Bitmap> cachedImages = mAutofillImageFetcher.getCachedImagesForTesting();

        // Verify that fetchImage was called twice (once for each image size).
        verify(mMockImageFetcher, times(2)).fetchImage(any(Params.class), any(Callback.class));

        // Each card art image is cached at 2 resolutions: 32x20 for the Keyboard Accessory, and
        // 40x24 on all other surfaces.
        assertEquals(2, cachedImages.size());
        assertTrue(treatedImageSmall.sameAs(cachedImages.get(imageCacheKeySmall.getSpec())));
        assertTrue(treatedImageLarge.sameAs(cachedImages.get(imageCacheKeyLarge.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_imageInCache_imageNotFetched() {
        CardIconSpecs cardIconSpecs =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.SMALL);
        GURL imageCacheKey =
                AutofillUiUtils.getFifeIconUrlWithParams(
                        TEST_IMAGE_URL, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        mAutofillImageFetcher.addImageToCacheForTesting(imageCacheKey, TEST_IMAGE);
        // No histogram should be logged since no image fetching is done.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {TEST_IMAGE_URL}, new int[] {ImageSize.SMALL});
        Map<String, Bitmap> cachedImages = mAutofillImageFetcher.getCachedImagesForTesting();

        // Verify that fetchImage was not called since the image is already in cache.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the cache contains only the already cached image.
        assertEquals(1, cachedImages.size());
        assertTrue(TEST_IMAGE.sameAs(cachedImages.get(imageCacheKey.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_validUrl_unsuccessfulImageFetch() {
        doAnswer(
                        invocation -> {
                            Callback callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(Params.class), any(Callback.class));
        // Both generic and credit card art image fetcher histograms should log failure. Since
        // fetching is attempted again, the generic histogram should log failure twice.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ false, /* times= */ 2)
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.CreditCardArt.Result",
                                /* value= */ false,
                                /* times= */ 1)
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {TEST_IMAGE_URL}, new int[] {ImageSize.SMALL});

        // Advance the clock to trigger the retry.
        mShadowLooper.runOneTask();
        // Advance the task again to make sure image fetching is retried only once.
        mShadowLooper.runOneTask();

        // Verify that fetchImage was called twice.
        verify(mMockImageFetcher, times(2)).fetchImage(any(Params.class), any(Callback.class));

        // Verify the image cache is empty.
        assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_validUrl_unsuccessfulImageFetch_successOnRetry() {
        // Use AtomicInteger to track the number of calls.
        AtomicInteger callCount = new AtomicInteger(0);
        // Make the first fetch fail, and the second succeed.
        doAnswer(
                        invocation -> {
                            Callback callback = invocation.getArgument(1);
                            if (callCount.getAndIncrement() == 0) {
                                callback.onResult(null);
                                return null;
                            }
                            callback.onResult(TEST_IMAGE);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(Params.class), any(Callback.class));
        CardIconSpecs cardIconSpecs =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.SMALL);
        GURL imageCacheKey =
                AutofillUiUtils.getFifeIconUrlWithParams(
                        TEST_IMAGE_URL, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        Bitmap treatedImage =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_IMAGE, cardIconSpecs, true);
        // The credit card art image fetcher histogram should log success. Since image fetching
        // succeeded after initially failing, the generic histogram should log both failure and
        // success.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ false, /* times= */ 1)
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ true, /* times= */ 1)
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.CreditCardArt.Result",
                                /* value= */ true,
                                /* times= */ 1)
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {TEST_IMAGE_URL}, new int[] {ImageSize.SMALL});
        Map<String, Bitmap> cachedImages = mAutofillImageFetcher.getCachedImagesForTesting();

        // Advance the clock to trigger the retry.
        mShadowLooper.runOneTask();

        // Verify that fetchImage was called twice.
        verify(mMockImageFetcher, times(2)).fetchImage(any(Params.class), any(Callback.class));

        // Verify the image cache contains the fetched image.
        assertEquals(1, cachedImages.size());
        assertTrue(treatedImage.sameAs(cachedImages.get(imageCacheKey.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_invalidOrEmptyUrl() {
        GURL invalidUrl = new GURL("invalid-image-url");
        GURL emptyUrl = new GURL("");
        // No histogram should be logged since image fetching isn't attempted for invalid URLs.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {invalidUrl, emptyUrl}, new int[] {ImageSize.SMALL});

        // Verify that fetchImage was not called for invalid URLs.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the image cache is empty.
        assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchCardArtImages_capitalOneStaticImageUrl_notFetched() {
        GURL capitalOneStaticImageUrl = new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL);
        // No histogram should be logged since image isn't fetched for Capital One's static card art
        // URL.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mAutofillImageFetcher.prefetchCardArtImages(
                new GURL[] {capitalOneStaticImageUrl}, new int[] {ImageSize.SMALL});

        // Verify that fetchImage was not called for Capital One's static card art URL.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the image cache is empty.
        assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_validUrl_successfulImageFetch() {
        GURL imageCacheKey =
                AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(TEST_IMAGE_URL);
        Bitmap treatedImage = AutofillImageFetcherUtils.treatPixAccountImage(TEST_IMAGE);
        // Success histograms should be logged for both images.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ true, /* times= */ 1)
                        .build();

        mAutofillImageFetcher.prefetchPixAccountImages(new GURL[] {TEST_IMAGE_URL});
        Map<String, Bitmap> cachedImages = mAutofillImageFetcher.getCachedImagesForTesting();

        // Verify that fetchImage was called once.
        verify(mMockImageFetcher).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the images are successfully fetched and cached.
        assertEquals(1, cachedImages.size());
        assertTrue(treatedImage.sameAs(cachedImages.get(imageCacheKey.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_imageInCache_imageNotFetched() {
        GURL imageCacheKey =
                AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(TEST_IMAGE_URL);
        mAutofillImageFetcher.addImageToCacheForTesting(imageCacheKey, TEST_IMAGE);
        // No histogram should be logged since no image fetching is done.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mAutofillImageFetcher.prefetchPixAccountImages(new GURL[] {TEST_IMAGE_URL});
        Map<String, Bitmap> cachedImages = mAutofillImageFetcher.getCachedImagesForTesting();

        // Verify that fetchImage was not called since the image is already in cache.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the cache contains only the already cached image.
        assertEquals(1, cachedImages.size());
        assertTrue(TEST_IMAGE.sameAs(cachedImages.get(imageCacheKey.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_validUrl_unsuccessfulImageFetch() {
        doAnswer(
                        invocation -> {
                            Callback callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(Params.class), any(Callback.class));
        // Failure histogram should be logged since fetching was attempted but failed.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ false, /* times= */ 1)
                        .build();

        mAutofillImageFetcher.prefetchPixAccountImages(new GURL[] {TEST_IMAGE_URL});

        // Verify that fetchImage was called once.
        verify(mMockImageFetcher).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the cache is empty since image fetching failed.
        assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_invalidOrEmptyUrl() {
        GURL invalidUrl = new GURL("invalid-image-url");
        GURL emptyUrl = new GURL("");
        // No histogram should be logged since image fetching isn't attempted for invalid URLs.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mAutofillImageFetcher.prefetchPixAccountImages(new GURL[] {invalidUrl, emptyUrl});

        // Verify that fetchImage was not called for invalid URLs.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        // Verify that the cache is empty since the image URLs weren't valid and no images were
        // fetched.
        assertTrue(mAutofillImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testGetPixAccountIcon_imageInCache() {
        mAutofillImageFetcher.addImageToCacheForTesting(
                AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(TEST_IMAGE_URL),
                TEST_IMAGE);

        Drawable pixAccountIcon =
                mAutofillImageFetcher.getPixAccountIcon(
                        ContextUtils.getApplicationContext(), TEST_IMAGE_URL);

        // Verify that fetchImage is never called from "get" methods.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        assertNotNull(pixAccountIcon);
        assertTrue(TEST_IMAGE.sameAs(drawableToBitmap(pixAccountIcon)));
    }

    @Test
    @SmallTest
    public void testGetPixAccountIcon_imageNotInCache() {
        Context context = ContextUtils.getApplicationContext();
        Drawable genericBankAccountIcon =
                AppCompatResources.getDrawable(context, R.drawable.ic_account_balance);
        Drawable pixAccountIcon = mAutofillImageFetcher.getPixAccountIcon(context, TEST_IMAGE_URL);

        // Verify that fetchImage is never called from "get" methods.
        verify(mMockImageFetcher, never()).fetchImage(any(Params.class), any(Callback.class));

        assertNotNull(pixAccountIcon);
        assertTrue(
                drawableToBitmap(genericBankAccountIcon).sameAs(drawableToBitmap(pixAccountIcon)));
    }

    private @Nullable Bitmap drawableToBitmap(Drawable drawable) {
        if (drawable == null) {
            return null;
        }

        if (drawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) drawable).getBitmap();
        }

        // Create a copy of the drawable in bitmap format.
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
