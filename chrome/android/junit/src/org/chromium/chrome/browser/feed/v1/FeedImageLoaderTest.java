// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalMatchers;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.BundledAssets;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.image_fetcher.CachedImageFetcher;

import java.util.Arrays;

/**
 * Unit tests for {@link FeedImageLoader}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedImageLoaderTest {
    private static final String HTTP_STRING1 = "http://www.test1.com";
    private static final String HTTP_STRING2 = "http://www.test2.com";
    private static final String HTTP_STRING3 = "http://www.test3.com";

    private static final String ASSET_PREFIX = "asset://";
    private static final String OFFLINE_ASSET_STRING =
            ASSET_PREFIX + BundledAssets.OFFLINE_INDICATOR_BADGE;
    private static final String VIDEO_ASSET_STRING =
            ASSET_PREFIX + BundledAssets.VIDEO_INDICATOR_BADGE;
    private static final String BAD_ASSET_STRING = ASSET_PREFIX + "does_not_exist";

    private static final String OVERLAY_IMAGE_START =
            "overlay-image://?direction=start&url=http://www.test1.com";
    private static final String OVERLAY_IMAGE_END =
            "overlay-image://?direction=end&url=http://www.test1.com";
    private static final String OVERLAY_IMAGE_MISSING_URL = "overlay-image://?direction=end";
    private static final String OVERLAY_IMAGE_MISSING_DIRECTION =
            "overlay-image://?url=http://www.test1.com";
    private static final String OVERLAY_IMAGE_BAD_DIRECTION =
            "overlay-image://?direction=east&url=http://www.test1.com";

    @Mock
    CachedImageFetcher mCachedImageFetcher;
    @Mock
    private Consumer<Drawable> mConsumer;
    @Mock
    private Bitmap mBitmap;
    @Captor
    ArgumentCaptor<Integer> mWidthPxCaptor;
    @Captor
    ArgumentCaptor<Integer> mHeightPxCaptor;
    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackArgument;

    private FeedImageLoader mImageLoader;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setUpWithImageFetcher(mCachedImageFetcher);
    }

    public void setUpWithImageFetcher(CachedImageFetcher cachedImageFetcher) {
        mImageLoader = Mockito.spy(
                new FeedImageLoader(ContextUtils.getApplicationContext(), cachedImageFetcher));
    }

    private void answerFetchImage(String url, Bitmap bitmap) {
        doAnswer((InvocationOnMock invocation) -> {
            mCallbackArgument.getValue().onResult(bitmap);
            return null;
        })
                .when(mImageLoader)
                .fetchImage(eq(url), mWidthPxCaptor.capture(), mHeightPxCaptor.capture(),
                        mCallbackArgument.capture());
    }

    private void loadDrawable(int widthPx, int heightPx, String... urls) {
        // While normally {@link FeedImageLoader#loadDrawable} guarantees that the return callback
        // is invoked asynchronously, this is not the case in tests. It seems that both
        // {@link FeedImageLoaderTest#answerFetchImage}, {@link AndroidThreadUtils.postOnUiThread}
        // run synchronously.
        mImageLoader.loadDrawable(Arrays.asList(urls), widthPx, heightPx, mConsumer);
    }

    private void loadDrawable(String... urls) {
        loadDrawable(ImageLoaderApi.DIMENSION_UNKNOWN, ImageLoaderApi.DIMENSION_UNKNOWN, urls);
    }

    @Test
    @SmallTest
    public void testLoadDrawable() {
        answerFetchImage(HTTP_STRING1, mBitmap);

        loadDrawable(100, 200, HTTP_STRING1);

        verify(mImageLoader, times(1)).fetchImage(eq(HTTP_STRING1), eq(100), eq(200), any());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadDrawableFailure() {
        answerFetchImage(HTTP_STRING1, null);

        loadDrawable(HTTP_STRING1);

        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test
    @SmallTest
    public void testLoadDrawableWithNullFetcher() {
        setUpWithImageFetcher(null);
        loadDrawable(HTTP_STRING1);
        verify(mConsumer, times(1)).accept(eq(null));
        verify(mImageLoader, times(0))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
    }

    @Test
    @SmallTest
    public void testLoadDrawableMultiple() {
        answerFetchImage(HTTP_STRING1, null);
        answerFetchImage(HTTP_STRING2, mBitmap);

        loadDrawable(HTTP_STRING1, HTTP_STRING2, HTTP_STRING3);

        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING2), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
        verify(mImageLoader, times(0))
                .fetchImage(eq(HTTP_STRING3), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadOfflineBadge() {
        loadDrawable(OFFLINE_ASSET_STRING);
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadVideoBadge() {
        loadDrawable(VIDEO_ASSET_STRING);
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadDrawableMissingAsset() {
        loadDrawable(BAD_ASSET_STRING);
        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test
    @SmallTest
    public void testLoadDrawableAssetFallback() {
        loadDrawable(BAD_ASSET_STRING, OFFLINE_ASSET_STRING);
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadDrawableAssetFirst() {
        loadDrawable(VIDEO_ASSET_STRING, HTTP_STRING1);
        verify(mImageLoader, times(0))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), any());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadDrawableEmptyList() {
        loadDrawable();
        verify(mImageLoader, times(0)).fetchImage(any(), anyInt(), anyInt(), any());
        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test
    @SmallTest
    public void testLoadDrawableOverlay_Start() {
        answerFetchImage(HTTP_STRING1, mBitmap);

        loadDrawable(OVERLAY_IMAGE_START);

        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), mCallbackArgument.capture());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void testLoadDrawableOverlay_End() {
        answerFetchImage(HTTP_STRING1, mBitmap);

        loadDrawable(OVERLAY_IMAGE_END);

        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), mCallbackArgument.capture());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testLoadDrawableOverlay_MissingUrl() {
        loadDrawable(OVERLAY_IMAGE_MISSING_URL);
    }

    @Test
    @SmallTest
    public void testLoadDrawableOverlay_Fallback() {
        answerFetchImage(HTTP_STRING1, null);
        answerFetchImage(HTTP_STRING2, mBitmap);

        loadDrawable(OVERLAY_IMAGE_END, HTTP_STRING2);

        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING1), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), mCallbackArgument.capture());
        verify(mImageLoader, times(1))
                .fetchImage(eq(HTTP_STRING2), eq(ImageLoaderApi.DIMENSION_UNKNOWN),
                        eq(ImageLoaderApi.DIMENSION_UNKNOWN), mCallbackArgument.capture());
        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testLoadDrawableOverlay_MissingDirection() {
        loadDrawable(OVERLAY_IMAGE_MISSING_DIRECTION);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void overlayImageTest_BadDirection() {
        loadDrawable(OVERLAY_IMAGE_BAD_DIRECTION);
    }
}
