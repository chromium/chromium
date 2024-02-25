// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.test.filters.SmallTest;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests {@link WebFeedFaviconFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class})
@SmallTest
public class WebFeedFaviconFetcherTest {
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL FAVICON_URL = JUnitTestGURLs.RED_1;

    private Bitmap mBitmapFromImageFetcher;
    private Bitmap mBitmapFromIconBridge;
    private Activity mActivity;
    private TestImageFetcher mImageFetcher = Mockito.spy(new TestImageFetcher());
    private TestLargeIconBridge mLargeIconBridge = Mockito.spy(new TestLargeIconBridge());
    private WebFeedFaviconFetcher mFaviconFetcher =
            new WebFeedFaviconFetcher(mLargeIconBridge, mImageFetcher);

    @Before
    public void setUp() {
        // Print logs to stdout.
        ShadowLog.stream = System.out;

        mActivity = Robolectric.setupActivity(Activity.class);
        mBitmapFromImageFetcher =
                Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
        mBitmapFromIconBridge =
                Bitmap.createBitmap(/* width= */ 2, /* height= */ 2, Bitmap.Config.ARGB_8888);
    }

    @Test
    public void beginFetch_withFaviconUrl_allFetchesFail_returnsMonogram() {
        AtomicBoolean callbackCalled = new AtomicBoolean(false);
        AtomicReference<Bitmap> returnedImage = new AtomicReference(null);
        mFaviconFetcher.beginFetch(
                3,
                1,
                TEST_URL,
                FAVICON_URL,
                (Bitmap bitmap) -> {
                    returnedImage.set(bitmap);
                    callbackCalled.set(true);
                });
        mImageFetcher.answerWithNull();
        mLargeIconBridge.answerWithNull();

        assertTrue(callbackCalled.get());
        assertBitmapIsMonogram(returnedImage.get());
        assertEquals(3, returnedImage.get().getWidth());
        assertEquals(3, returnedImage.get().getHeight());
    }

    @Test
    public void beginFetch_withFaviconUrl_successfulImageFetch() {
        AtomicBoolean callbackCalled = new AtomicBoolean(false);
        AtomicReference<Bitmap> returnedImage = new AtomicReference(null);
        mFaviconFetcher.beginFetch(
                1,
                1,
                TEST_URL,
                FAVICON_URL,
                (Bitmap bitmap) -> {
                    returnedImage.set(bitmap);
                    callbackCalled.set(true);
                });
        mImageFetcher.answerWithBitmap();

        assertTrue(callbackCalled.get());
        assertEquals(mBitmapFromImageFetcher, returnedImage.get());
    }

    @Test
    public void beginFetch_withFaviconUrl_failedImageFetch_successfulIconFetch() {
        AtomicBoolean callbackCalled = new AtomicBoolean(false);
        AtomicReference<Bitmap> returnedImage = new AtomicReference(null);
        mFaviconFetcher.beginFetch(
                1,
                1,
                TEST_URL,
                FAVICON_URL,
                (Bitmap bitmap) -> {
                    returnedImage.set(bitmap);
                    callbackCalled.set(true);
                });
        mImageFetcher.answerWithNull();
        mLargeIconBridge.answerWithBitmap();

        assertTrue(callbackCalled.get());
        assertEquals(mBitmapFromIconBridge, returnedImage.get());
    }

    @Test
    public void beginFetch_withoutFaviconUrl_allFetchesFail_returnsMonogram() {
        AtomicBoolean callbackCalled = new AtomicBoolean(false);
        AtomicReference<Bitmap> returnedImage = new AtomicReference(null);
        mFaviconFetcher.beginFetch(
                1,
                1,
                TEST_URL,
                null,
                (Bitmap bitmap) -> {
                    returnedImage.set(bitmap);
                    callbackCalled.set(true);
                });
        mLargeIconBridge.answerWithNull();

        assertTrue(callbackCalled.get());
        assertBitmapIsMonogram(returnedImage.get());
    }

    @Test
    public void beginFetch_withInvalidFaviconUrl_successfulIconFetch() {
        AtomicBoolean callbackCalled = new AtomicBoolean(false);
        AtomicReference<Bitmap> returnedImage = new AtomicReference(null);
        mFaviconFetcher.beginFetch(
                1,
                1,
                TEST_URL,
                GURL.emptyGURL(),
                (Bitmap bitmap) -> {
                    returnedImage.set(bitmap);
                    callbackCalled.set(true);
                });
        mLargeIconBridge.answerWithBitmap();

        assertTrue(callbackCalled.get());
        assertEquals(mBitmapFromIconBridge, returnedImage.get());
    }

    void assertBitmapIsMonogram(Bitmap bitmap) {
        // Assume the bitmap came from RoundedIconGenerator if it's not null and not
        // from the other bitmap sources.
        assertNotEquals(null, bitmap);
        assertNotEquals(mBitmapFromImageFetcher, bitmap);
        assertNotEquals(mBitmapFromIconBridge, bitmap);
    }

    class TestImageFetcher extends ImageFetcher.ImageFetcherForTesting {
        private Callback<Bitmap> mCallback;

        private void answerWithBitmap() {
            mCallback.onResult(mBitmapFromImageFetcher);
            mCallback = null;
        }

        private void answerWithNull() {
            mCallback.onResult(null);
            mCallback = null;
        }

        @Override
        public void fetchImage(final ImageFetcher.Params params, Callback<Bitmap> callback) {
            mCallback = callback;
        }

        @Override
        public void fetchGif(final ImageFetcher.Params params, Callback<BaseGifImage> callback) {}

        @Override
        public void clear() {}

        @Override
        public @ImageFetcherConfig int getConfig() {
            return ImageFetcherConfig.IN_MEMORY_ONLY;
        }

        @Override
        public void destroy() {}
    }

    private class TestLargeIconBridge extends LargeIconBridge {
        private LargeIconCallback mCallback;

        @Override
        public boolean getLargeIconForUrl(
                final GURL pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            mCallback = callback;
            return true;
        }

        public void answerWithBitmap() {
            mCallback.onLargeIconAvailable(
                    mBitmapFromIconBridge, Color.BLACK, false, IconType.INVALID);
            mCallback = null;
        }

        public void answerWithNull() {
            mCallback.onLargeIconAvailable(null, Color.BLACK, false, IconType.INVALID);
            mCallback = null;
        }
    }
}
