// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Test for ImageFetcher.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherTest {
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;

    /**
     * Concrete implementation for testing purposes.
     */
    private class ImageFetcherForTest extends ImageFetcher {
        @Override
        public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {}

        @Override
        public void fetchImage(
                String url, String clientName, int width, int height, Callback<Bitmap> callback) {}

        @Override
        public void clear() {}

        @Override
        public int getConfig() {
            return 0;
        }

        @Override
        public void destroy() {}
    }

    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    private ImageFetcherForTest mImageFetcher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mImageFetcher = Mockito.spy(new ImageFetcherForTest());
    }

    @Test
    public void testResize() {
        Bitmap result = ImageFetcher.tryToResizeImage(mBitmap, WIDTH_PX / 2, HEIGHT_PX / 2);
        assertNotEquals(result, mBitmap);
    }

    @Test
    public void testResizeBailsOutIfSizeIsZeroOrLess() {
        Bitmap result = ImageFetcher.tryToResizeImage(mBitmap, WIDTH_PX - 1, HEIGHT_PX - 1);
        assertNotEquals(result, mBitmap);

        result = ImageFetcher.tryToResizeImage(mBitmap, 0, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.tryToResizeImage(mBitmap, WIDTH_PX, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.tryToResizeImage(mBitmap, 0, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.tryToResizeImage(mBitmap, -1, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.tryToResizeImage(mBitmap, WIDTH_PX, -1);
        assertEquals(result, mBitmap);
    }

    @Test
    public void testFetchImageNoDimensionsAlias() {
        String url = "url";
        String client = "client";
        mImageFetcher.fetchImage(url, client, result -> {});

        // No arguments should alias to 0, 0.
        verify(mImageFetcher).fetchImage(eq(url), eq(client), eq(0), eq(0), any());
    }
}
