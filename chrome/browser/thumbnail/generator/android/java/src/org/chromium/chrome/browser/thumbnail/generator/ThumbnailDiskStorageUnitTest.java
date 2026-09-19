// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.util.ArrayList;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit test for ThumbnailProviderDiskStorage. */
@RunWith(BaseRobolectricTestRunner.class)
// Needed so Bitmap.compress(PNG) and Bitmap.sameAs() operate on real Skia pixel buffers.
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class ThumbnailDiskStorageUnitTest {
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
        private final String mContentId;

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
        public final AtomicInteger removeCount = new AtomicInteger();

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

            ArrayList<Pair<String, Integer>> list = new ArrayList<>(sDiskLruCache);
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
        ThumbnailDiskStorage.sDiskLruCache.clear();
        ThumbnailDiskStorage.sIconSizesMap.clear();
        mTestThumbnailDiskStorage = new TestThumbnailDiskStorage(mTestThumbnailGenerator);
        // Clear the disk cache so that preexisting cached files do not interrupt test execution.
        mTestThumbnailDiskStorage.clear();
        try {
            RobolectricUtil.runAllBackgroundAndUiAllowBlocking();
            mTestThumbnailDiskStorage.mInitTask.get();
            mTestThumbnailDiskStorage.mLastClearTask.get();
        } catch (Exception e) {
            throw new RuntimeException("Exception occurred while waiting for task.", e);
        }
        mTestThumbnailDiskStorage.removeCount.set(0);
        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that an inserted thumbnail can be retrieved. */
    @Test
    public void testCanInsertAndGet() {
        addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        assertEquals(1, mTestThumbnailDiskStorage.getCacheCount());

        TestThumbnailRequest request = new TestThumbnailRequest(CONTENT_ID1);
        retrieveThumbnailAndAssertRetrieved(request);

        // Ensure the thumbnail generator is not called.
        assertEquals(0, mTestThumbnailGenerator.generateCount.get());
        Bitmap bitmap = getFromDisk(CONTENT_ID1, ICON_WIDTH1);
        assertNotNull(bitmap);
        assertTrue(bitmap.sameAs(BITMAP1));

        // Since retrieval re-adds an existing entry, remove was called once already.
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /**
     * Verify that two inserted entries with the same key (content ID) will count as only one entry
     * and the first entry data will be replaced with the second.
     */
    @Test
    public void testRepeatedInsertShouldBeUpdated() {
        addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        addToDisk(CONTENT_ID1, BITMAP2, ICON_WIDTH1);

        // Verify that the old entry is updated with the new
        assertEquals(1, mTestThumbnailDiskStorage.getCacheCount());
        Bitmap bitmap = getFromDisk(CONTENT_ID1, ICON_WIDTH1);
        assertNotNull(bitmap);
        assertTrue(bitmap.sameAs(BITMAP2));

        // Note: since an existing entry is re-added, remove was called once already
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that retrieveThumbnail makes the called entry the most recent entry in cache. */
    @Test
    public void testRetrieveThumbnailShouldMakeEntryMostRecent() {
        addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        addToDisk(CONTENT_ID2, BITMAP1, ICON_WIDTH1);
        addToDisk(CONTENT_ID3, BITMAP1, ICON_WIDTH1);
        assertEquals(3, mTestThumbnailDiskStorage.getCacheCount());

        // Verify no trimming is done
        assertEquals(0, mTestThumbnailDiskStorage.removeCount.get());

        TestThumbnailRequest request = new TestThumbnailRequest(CONTENT_ID1);
        retrieveThumbnailAndAssertRetrieved(request);
        assertEquals(mTestThumbnailGenerator.generateCount.get(), 0);

        // Since retrieval re-adds an existing entry, remove was called once already
        assertEquals(1, mTestThumbnailDiskStorage.removeCount.get());

        // Verify that the called entry is the most recent entry
        assertTrue(
                mTestThumbnailDiskStorage
                        .getMostRecentEntry()
                        .equals(Pair.create(CONTENT_ID1, ICON_WIDTH1)));

        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        removeThumbnailAndExpectedCount(CONTENT_ID2, 3);
        removeThumbnailAndExpectedCount(CONTENT_ID3, 4);
        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /** Verify that trim removes the least recently used entry. */
    @Test
    public void testExceedLimitShouldTrim() {
        // Add thumbnails up to cache limit to get 1 entry trimmed
        int count = 0;
        while (mTestThumbnailDiskStorage.removeCount.get() == 0) {
            addToDisk("contentId" + count, BITMAP1, ICON_WIDTH1);
            ++count;
        }

        // Since count includes the oldest entry trimmed, verify that cache size is one less
        assertEquals(count - 1, mTestThumbnailDiskStorage.getCacheCount());
        // The oldest entry was contentId0 before trim and should now be contentId1.
        assertTrue(
                mTestThumbnailDiskStorage
                        .getOldestEntry()
                        .equals(Pair.create(CONTENT_ID1, ICON_WIDTH1)));

        // Since contentId0 has been removed, {@code i} should start at 1 and removeCount is now 1.
        for (int i = 1; i <= count - 1; i++) {
            removeThumbnailAndExpectedCount("contentId" + i, i + 1);
        }
        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    /**
     * Verify that removeFromDisk removes all thumbnails with the same content ID but different
     * sizes.
     */
    @Test
    public void testRemoveAllThumbnailsWithSameContentId() {
        addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH1);
        addToDisk(CONTENT_ID1, BITMAP1, ICON_WIDTH2);
        assertEquals(2, mTestThumbnailDiskStorage.getCacheCount());
        assertEquals(2, getIconSizes(CONTENT_ID1).size());

        // Expect two removals from cache for the two thumbnails
        removeThumbnailAndExpectedCount(CONTENT_ID1, 2);
        assertEquals(0, mTestThumbnailDiskStorage.getCacheCount());
        assertTrue(getIconSizes(CONTENT_ID1) == null);

        assertEquals(0, mTestThumbnailDiskStorage.mSizeBytes);
    }

    private void addToDisk(String contentId, Bitmap bitmap, int iconSizePx) {
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> mTestThumbnailDiskStorage.addToDisk(contentId, bitmap, iconSizePx));
        RobolectricUtil.runAllBackgroundAndUiAllowBlocking();
    }

    private @Nullable Bitmap getFromDisk(String contentId, int iconSizePx) {
        AtomicReference<Bitmap> result = new AtomicReference<>();
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> result.set(mTestThumbnailDiskStorage.getFromDisk(contentId, iconSizePx)));
        RobolectricUtil.runAllBackgroundAndUiAllowBlocking();
        return result.get();
    }

    /** Retrieve thumbnail and assert that {@link ThumbnailStorageDelegate} has received it. */
    private void retrieveThumbnailAndAssertRetrieved(final TestThumbnailRequest request) {
        mTestThumbnailDiskStorage.retrieveThumbnail(request);

        try {
            // This tasks calls onThumbnailRetrieved which creates a CacheThumbnailTask.
            RobolectricUtil.runAllBackgroundAndUiAllowBlocking();
            mTestThumbnailDiskStorage.mLastGetThumbnailTask.get();
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
        mTestThumbnailDiskStorage.removeFromDisk(contentId);
        try {
            RobolectricUtil.runAllBackgroundAndUiAllowBlocking();
            mTestThumbnailDiskStorage.mLastRemoveThumbnailTask.get();
        } catch (Exception e) {
            throw new RuntimeException("Exception occurred while waiting for task.", e);
        }
        assertEquals(expectedRemoveCount, mTestThumbnailDiskStorage.removeCount.get());
    }

    private Set<Integer> getIconSizes(String contentId) {
        return ThumbnailDiskStorage.sIconSizesMap.get(contentId);
    }
}
