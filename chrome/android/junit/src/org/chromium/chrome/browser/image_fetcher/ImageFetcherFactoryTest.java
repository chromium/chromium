// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Test for ImageFetcherFactory.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherFactoryTest {
    @Mock
    ImageFetcherBridge mImageFetcherBridge;
    @Mock
    DiscardableReferencePool mReferencePool;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testGetImageFetcher() {
        assertEquals(ImageFetcherConfig.NETWORK_ONLY,
                ImageFetcherFactory
                        .createImageFetcher(ImageFetcherConfig.NETWORK_ONLY, mImageFetcherBridge,
                                mReferencePool, InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(ImageFetcherConfig.DISK_CACHE_ONLY,
                ImageFetcherFactory
                        .createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY, mImageFetcherBridge,
                                mReferencePool, InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(ImageFetcherConfig.IN_MEMORY_ONLY,
                ImageFetcherFactory
                        .createImageFetcher(ImageFetcherConfig.IN_MEMORY_ONLY, mImageFetcherBridge,
                                mReferencePool, InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                ImageFetcherFactory
                        .createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                mImageFetcherBridge, mReferencePool,
                                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
    }
}
