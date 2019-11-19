// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link BitmapCache}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BitmapCacheTest {
    private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
    private static final int MAX_CACHE_BYTES = 5 * 1024 * 1024;

    private final Bitmap mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    private final Bitmap mBitmap2 = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
    private static final String KEY = "123";
    private static final String KEY2 = "another";

    @Before
    public void setUp() {
        BitmapCache.clearDedupCacheForTesting();
    }

    @Test
    public void testCapacity() {
        Assert.assertTrue(MAX_CACHE_BYTES >= mBitmap.getByteCount());
    }

    @Test
    public void testGetBitmap() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        Assert.assertEquals(mBitmap, cache.getBitmap(KEY));
    }

    @Test
    public void testOverwrite() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        cache.putBitmap(KEY, mBitmap2);
        Assert.assertEquals(mBitmap2, cache.getBitmap(KEY));
    }

    @Test
    public void testEmpty() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        Assert.assertNull(cache.getBitmap(KEY));
    }

    @Test
    public void testGetNonexisting() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        Assert.assertNull(cache.getBitmap(KEY2));
    }

    @Test
    public void testDedupGetBitmap() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        BitmapCache cache2 = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        Assert.assertEquals(mBitmap, cache2.getBitmap(KEY));
    }

    @Test
    public void testDedupSize() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        BitmapCache cache2 = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        cache2.putBitmap(KEY, mBitmap);
        Assert.assertEquals(1, BitmapCache.dedupCacheSizeForTesting());
    }

    @Test
    public void testDedupOverwrite() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        BitmapCache cache2 = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        BitmapCache cache3 = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        cache2.putBitmap(KEY, mBitmap2);
        Assert.assertEquals(mBitmap, cache.getBitmap(KEY));
        Assert.assertEquals(mBitmap2, cache2.getBitmap(KEY));
        Assert.assertEquals(mBitmap2, cache3.getBitmap(KEY));
        Assert.assertEquals(1, BitmapCache.dedupCacheSizeForTesting());
    }

    /**
     * {@link BitmapCache#clearDedupCacheForTesting} is supposed to be called in
     * setUp() only, and should not be called inside a @Test, or it would leak
     * too much implementation details. However, it is required to do it to
     * properly test the cache eviction without relying on GC. In order to make
     * sure {@link #testLowCapacity} tests what we want to test, this test verifies
     * that calling {@link BitmapCache#clearDedupCacheForTesting} is not the reason
     * the cache returns null.
     */
    @Test
    public void testLowCapacityRef() {
        BitmapCache cache = new BitmapCache(mReferencePool, MAX_CACHE_BYTES);
        cache.putBitmap(KEY, mBitmap);
        BitmapCache.clearDedupCacheForTesting();
        Assert.assertEquals(mBitmap, cache.getBitmap(KEY));
    }

    @Test
    public void testLowCapacity() {
        BitmapCache cache = new BitmapCache(mReferencePool, 1); // The size cannot be 0.
        cache.putBitmap(KEY, mBitmap);
        BitmapCache.clearDedupCacheForTesting();
        Assert.assertNull(cache.getBitmap(KEY));
    }

    @Test
    public void testLru() {
        BitmapCache cache = new BitmapCache(
                mReferencePool, mBitmap.getByteCount() + mBitmap2.getByteCount() - 1);

        cache.putBitmap(KEY, mBitmap);
        BitmapCache.clearDedupCacheForTesting();
        Assert.assertEquals(mBitmap, cache.getBitmap(KEY));

        cache.putBitmap(KEY2, mBitmap2);
        BitmapCache.clearDedupCacheForTesting();
        Assert.assertEquals(mBitmap2, cache.getBitmap(KEY2));
        Assert.assertNull(cache.getBitmap(KEY));

        cache.putBitmap(KEY, mBitmap);
        BitmapCache.clearDedupCacheForTesting();
        Assert.assertEquals(mBitmap, cache.getBitmap(KEY));
        Assert.assertNull(cache.getBitmap(KEY2));
    }
}
