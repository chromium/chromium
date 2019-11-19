// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Unit tests for CachedImageFetcher.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CachedImageFetcherTest {
    private static final String UMA_CLIENT_NAME = "TestUmaClient";
    private static final String URL = "http://foo.bar";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final long START_TIME = 274127;

    CachedImageFetcher mCachedImageFetcher;

    @Mock
    ImageFetcherBridge mImageFetcherBridge;
    @Mock
    Bitmap mBitmap;
    @Mock
    BaseGifImage mGif;

    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ImageFetcherBridge.setupForTesting(mImageFetcherBridge);
        mCachedImageFetcher = Mockito.spy(new CachedImageFetcher(mImageFetcherBridge));
        Mockito.doReturn(URL).when(mImageFetcherBridge).getFilePath(anyObject());
        doAnswer((InvocationOnMock invocation) -> {
            mCallbackCaptor.getValue().onResult(mBitmap);
            return null;
        })
                .when(mImageFetcherBridge)
                .fetchImage(anyInt(), eq(URL), eq(UMA_CLIENT_NAME), anyInt(), anyInt(),
                        mCallbackCaptor.capture());
    }

    @Test
    public void testFetchImageWithDimensionsFoundOnDisk() {
        mCachedImageFetcher.continueFetchImageAfterDisk(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX,
                (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); }, mBitmap, START_TIME);

        verify(mImageFetcherBridge, never()) // Should never make it to native.
                .fetchImage(anyInt(), eq(URL), eq(UMA_CLIENT_NAME), anyInt(), anyInt(), any());

        // Verify metrics have been reported.
        verify(mImageFetcherBridge)
                .reportEvent(eq(UMA_CLIENT_NAME), eq(ImageFetcherEvent.JAVA_DISK_CACHE_HIT));
        verify(mImageFetcherBridge).reportCacheHitTime(eq(UMA_CLIENT_NAME), eq(START_TIME));
    }

    @Test
    public void testFetchImageWithDimensionsCallToNative() {
        mCachedImageFetcher.continueFetchImageAfterDisk(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX,
                (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); }, null, START_TIME);

        verify(mImageFetcherBridge)
                .fetchImage(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(URL), eq(UMA_CLIENT_NAME),
                        anyInt(), anyInt(), any());
    }

    @Test
    public void testFetchTwoClients() {
        Mockito.doReturn(null).when(mCachedImageFetcher).tryToLoadImageFromDisk(anyObject());
        mCachedImageFetcher.continueFetchImageAfterDisk(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX,
                (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); }, null, START_TIME);
        mCachedImageFetcher.continueFetchImageAfterDisk(URL, UMA_CLIENT_NAME + "2", WIDTH_PX,
                HEIGHT_PX, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); }, null, START_TIME);

        verify(mImageFetcherBridge)
                .fetchImage(anyInt(), eq(URL), eq(UMA_CLIENT_NAME), anyInt(), anyInt(), any());
        verify(mImageFetcherBridge)
                .fetchImage(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(URL),
                        eq(UMA_CLIENT_NAME + "2"), anyInt(), anyInt(), any());
    }

    @Test
    public void testFetchGifFoundOnDisk() {
        mCachedImageFetcher.continueFetchGifAfterDisk(URL, UMA_CLIENT_NAME,
                (BaseGifImage gif) -> { assertEquals(gif, mGif); }, mGif, START_TIME);

        verify(mImageFetcherBridge, never()) // Should never make it to native.
                .fetchGif(anyInt(), eq(URL), eq(UMA_CLIENT_NAME), any());

        // Verify metrics have been reported.
        verify(mImageFetcherBridge)
                .reportEvent(eq(UMA_CLIENT_NAME), eq(ImageFetcherEvent.JAVA_DISK_CACHE_HIT));
        verify(mImageFetcherBridge).reportCacheHitTime(eq(UMA_CLIENT_NAME), anyLong());
    }

    @Test
    public void testFetchGifCallToNative() {
        mCachedImageFetcher.continueFetchGifAfterDisk(URL, UMA_CLIENT_NAME,
                (BaseGifImage gif) -> { assertEquals(gif, mGif); }, null, START_TIME);

        verify(mImageFetcherBridge)
                .fetchGif(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(URL), eq(UMA_CLIENT_NAME),
                        any());
    }
}
