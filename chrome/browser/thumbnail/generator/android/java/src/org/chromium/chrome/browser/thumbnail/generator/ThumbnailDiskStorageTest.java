// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.util.ArrayList;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit test for ThumbnailProviderDiskStorage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ThumbnailDiskStorageTest {
    private static final String CONTENT_ID1 = "contentId1";
    private static final String CONTENT_ID2 = "contentId2";
    private static final String CONTENT_ID3 = "contentId3";
    private static final int ICON_WIDTH1 = 50;
    private static final int ICON_WIDTH2 = 70;
    private static final Bitmap BITMAP1 =
            Bitmap.createBitmap(ICON_WIDTH1, ICON_WIDTH1, Bitmap.Config.ARGB_8888);
    private static final Bitmap BITMAP2 =
            Bitmap.createBitmap(ICON_WIDTH2, ICON_WIDTH2, Bitmap.Config.ARGB_8888);
    private static final int TEST_MAX_CACHE_BYTES = 10 * ConversionUtils.BYTES_PER_KILOBYTE;

    private TestThumbnailGenerator mTestThumbnailGenerator;
    private TestThumbnailDiskStorage mTestThumbnailDiskStorage;

    private static class TestThumbnailRequest implements ThumbnailProvider.ThumbnailRequest {
        private String mContentId;

        public TestThumbnailRequest(String contentId) {
            mContentId = contentId;
        }

        // This is not called in the test.
        @Override
        public String getFilePath() {
            return null;
        }

        @Override
        public String getMimeType() {
            return null;
        }

        @Override
        public String getContentId() {
            return mContentId;
        }

        @Override
        public void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap thumbnail) {}

        @Override
        public int getIconSize() {
            return ICON_WIDTH1;
        }
    }

    private static class TestThumbnailDiskStorage extends ThumbnailDiskStorage {
        // Incremented when adding an existing entry and trimming. Accessed by test and UI threads.
        public AtomicInteger removeCount = new AtomicInteger();

        public TestThumbnailDiskStorage(TestThumbnailGenerator thumbnailGenerator) {
            super(new ThumbnailStorageDelegate() {}, thumbnailGenerator, TEST_MAX_CACHE_BYTES);
        }

        @Override
        public void removeFromDiskHelper(Pair<String, Integer> contentIdSizePair) {
            removeCount.getAndIncrement();
            super.removeFromDiskHelper(contentIdSizePair);
        }

        /** The number of entries in the disk cache. Accessed in testing thread. */
        int getCacheCount() {
            return sDiskLruCache.size();
        }

        public Pair<String, Integer> getOldestEntry() {
            if (getCacheCount() <= 0) return null;

            return sDiskLruCache.iterator().next();
        }

        public Pair<String, Integer> getMostRecentEntry() {
            if (getCacheCount() <= 0) return null;

            ArrayList<Pair<String, Integer>> list =
                    new ArrayList<Pair<String, Integer>>(sDiskLruCache);
            return list.get(list.size() - 1);
        }
    }

    /** Dummy thumbnail generator that calls back immediately. */
    private static class TestThumbnailGenerator extends ThumbnailGenerator {
        // Accessed by test and UI threads.
        public final AtomicInteger generateCount = new AtomicInteger();

        @Override
        public void retrieveThumbnail(
                ThumbnailProvider.ThumbnailRequest request, ThumbnailGeneratorCallback callback) {
            generateCount.getAndIncrement();
            onThumbnailRetrieved(request.getContentId(), request.getIconSize(), null, callback);
        }
    }

    @Before
    public void setUp() {
        mTestThumbnailGenerator = new TestThumbnailGenerator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestThumbnailDiskStorage =
                            new TestThumbnailDiskStorage(mTestThumbnailGenerator);
                    // Clear the disk cache so that cached entries from previous runs won't show up.
                    mTestThumbnailDiskStorage.clear();
                });
        try {
            // Use .get() to ensure init and clear are completed. Since they have no onPostExecute
            // they are completely finished once .get() returns.
            mTestThumbnailDiskStorage.mInitTask.get();
            mTestThumbnailDiskStorage.mLastClearTask.get();
        } catch (Exception e) {
            throw new RuntimeException("Exception occurred while waiting for task.", e);
        }
        mTestThumbnailDiskStorage.removeCount.set(0);
        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that an inserted thumbnail can be retrieved. */
    @Test
    @SmallTest
    public void testCanInsertAndGet() {
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        Assert.assertEquals(1, mTestThumbnailDiskStorage.getCacheCount());

        TestThumbnailRequest request = new TestThumbnailRequest(CONTENT_ID1);
        retrieveThumbnailAndAssertRetrieved(request);

        // Ensure the thumbnail generator is not called.
        Assert.assertEquals(0, mTestThumbnailGenerator.generateCount.get());
        Assert.assertTrue(
                mTestThumbnailDiskStorage.getFromDisk(CONTENT_ID1, ICON_WIDTH1).sameAs(BITMAP1));

        // Since retrieval re-adds an existing entry, remove was called once already.
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /**
     * Verify that two inserted entries with the same key (content ID) will count as only one entry
     * and the first entry data will be replaced with the second.
     */
    @Test
    @SmallTest
    public void testRepeatedInsertShouldBeUpdated() {
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP2, ICON_WIDTH1);

        // Verify that the old entry is updated with the new
        Assert.assertEquals(1, mTestThumbnailDiskStorage.getCacheCount());
        Assert.assertTrue(
                mTestThumbnailDiskStorage.getFromDisk(CONTENT_ID1, ICON_WIDTH1).sameAs(BITMAP2));

        // Note: since an existing entry is re-added, remove was called once already
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that retrieveThumbnail makes the called entry the most recent entry in cache. */
    @Test
    @SmallTest
    public void testRetrieveThumbnailShouldMakeEntryMostRecent() {
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID2, BITMAP1, ICON_WIDTH1);
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID3, BITMAP1, ICON_WIDTH1);
        Assert.assertEquals(3, mTestThumbnailDiskStorage.getCacheCount());

        // Verify no trimming is done
        Assert.assertEquals(0, mTestThumbnailDiskStorage.removeCount.get());

        TestThumbnailRequest request = new TestThumbnailRequest(CONTENT_ID1);
        retrieveThumbnailAndAssertRetrieved(request);
        Assert.assertEquals(mTestThumbnailGenerator.generateCount.get(), 0);

        // Since retrieval re-adds an existing entry, remove was called once already
        Assert.assertEquals(1, mTestThumbnailDiskStorage.removeCount.get());

        // Verify that the called entry is the most recent entry
        Assert.assertTrue(
                mTestThumbnailDiskStorage
                        .getMostRecentEntry()
                        .equals(Pair.create(CONTENT_ID1, ICON_WIDTH1)));

        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        removeThumbnailAndExpectedCount(CONTENT_ID2, 3);
        removeThumbnailAndExpectedCount(CONTENT_ID3, 4);
        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that trim removes the least recently used entry. */
    @Test
    @SmallTest
    public void testExceedLimitShouldTrim() {
        // Add thumbnails up to cache limit to get 1 entry trimmed
        int count = 0;
        while (mTestThumbnailDiskStorage.removeCount.get() == 0) {
            mTestThumbnailDiskStorage.addToDisk("contentId" + count, BITMAP1, ICON_WIDTH1);
            ++count;
        }

        // Since count includes the oldest entry trimmed, verify that cache size is one less
        Assert.assertEquals(count - 1, mTestThumbnailDiskStorage.getCacheCount());
        // The oldest entry was contentId0 before trim and should now be contentId1.
        Assert.assertTrue(
                mTestThumbnailDiskStorage
                        .getOldestEntry()
                        .equals(Pair.create(CONTENT_ID1, ICON_WIDTH1)));

        // Since contentId0 has been removed, {@code i} should start at 1 and removeCount is now 1.
        for (int i = 1; i <= count - 1; i++) {
            removeThumbnailAndExpectedCount("contentId" + i, i + 1);
        }
        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /**
     * Verify that removeFromDisk removes all thumbnails with the same content ID but different
     * sizes.
     */
    @Test
    @SmallTest
    public void testRemoveAllThumbnailsWithSameContentId() {
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        mTestThumbnailDiskStorage.addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH2);
        Assert.assertEquals(2, mTestThumbnailDiskStorage.getCacheCount());
        Assert.assertEquals(2, getIconSizes(CONTENT_ID1).size());

        // Expect two removals from cache for the two thumbnails
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        Assert.assertEquals(0, mTestThumbnailDiskStorage.getCacheCount());
        Assert.assertTrue(getIconSizes(CONTENT_ID1) == null);

        Assert.assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Retrieve thumbnail and assert that {@link ThumbnailStorageDelegate} has received it. */
    private void retrieveThumbnailAndAssertRetrieved(final TestThumbnailRequest request) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestThumbnailDiskStorage.retrieveThumbnail(request));

        try {
            // This tasks calls onThumbnailRetrieved which creates a CacheThumbnailTask.
            mTestThumbnailDiskStorage.mLastGetThumbnailTask.get();
            // Since AsyncTask runs GetThumbnailTask's onPostExecute in a non-blocking way, we need
            // to ensure that it has finished calling onThumbnailRetrieved and created the cache
            // task before waiting on it. Use a short poll time since it usually doesn't take very
            // long to complete onPostExecute.
            CriteriaHelper.pollInstrumentationThread(
                    () -> mTestThumbnailDiskStorage.mLastCacheThumbnailTask != null);
            // Ensure that the cache is up-to-date before considering the thumbnail "retrieved".
            mTestThumbnailDiskStorage.mLastCacheThumbnailTask.get();
        } catch (Exception e) {
            throw new RuntimeException("Exception occurred while waiting for task.", e);
        }
    }

    /**
     * Remove thumbnail and ensure removal is completed.
     *
     * @param contentId Content ID of the thumbnail to remove
     * @param expectedRemoveCount The expected removeCount.
     */
    private void removeThumbnailAndExpectedCount(String contentId, int expectedRemoveCount) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestThumbnailDiskStorage.removeFromDisk(contentId));
        try {
            mTestThumbnailDiskStorage.mLastRemoveThumbnailTask.get();
        } catch (Exception e) {
            throw new RuntimeException("Exception occurred while waiting for task.", e);
        }
        Assert.assertEquals(expectedRemoveCount, mTestThumbnailDiskStorage.removeCount.get());
    }

    private Set<Integer> getIconSizes(String contentId) {
        return ThumbnailDiskStorage.sIconSizesMap.get(contentId);
    }
}
