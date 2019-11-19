// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v4.util.AtomicFile;
import android.support.v4.util.Pair;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.widget.ThumbnailCacheEntry.ContentId;
import org.chromium.chrome.browser.widget.ThumbnailCacheEntry.ThumbnailEntry;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;

/**
 * This class is a LRU cache of thumbnails on the disk and calls back to
 * {@link ThumbnailProviderImpl}. Thumbnails are shared across all
 * {@link ThumbnailProviderDiskStorage}s. There can be multiple
 * {@link ThumbnailProvider.ThumbnailRequest} being processed at a time, but all AsyncTasks are
 * executed serially. Missing thumbnails are retrieved by {@link ThumbnailProviderGenerator}.
 *
 * The caller should use {@link ThumbnailDiskStorage#create()} to create an instance.
 *
 * This class removes thumbnails from disk only if the file was removed in Download Home. It relies
 * on restart (when initDiskCache is called) and trim to sync to disk if file was removed
 * elsewhere (e.g. manually from disk).
 */
public class ThumbnailDiskStorage implements ThumbnailGeneratorCallback {
    private static final String TAG = "ThumbnailStorage";
    private static final int MAX_CACHE_BYTES =
            5 * ConversionUtils.BYTES_PER_MEGABYTE; // Max disk cache size is 5MB.

    // LRU cache of a pair of thumbnail's contentID and size. The order is based on the sequence of
    // add and get with the most recent at the end. The order at initialization (i.e. browser
    // restart) is based on the order of files in the directory. It is accessed only on the
    // background thread.
    // It is static because cached thumbnails are shared across all instances of the class.
    @VisibleForTesting
    static final LinkedHashSet<Pair<String, Integer>> sDiskLruCache =
            new LinkedHashSet<Pair<String, Integer>>();

    // Maps content ID to a set of the requested sizes (maximum required dimension of the smaller
    // side) of the thumbnail with that ID.
    @VisibleForTesting
    static final HashMap<String, HashSet<Integer>> sIconSizesMap =
            new HashMap<String, HashSet<Integer>>();

    @VisibleForTesting
    final ThumbnailGenerator mThumbnailGenerator;

    // This should be initialized once.
    private File mDirectory;

    private ThumbnailStorageDelegate mDelegate;

    // Maximum size in bytes for the disk cache.
    private final int mMaxCacheBytes;

    // Number of bytes used in disk for cache.
    @VisibleForTesting
    long mSizeBytes;

    // Whether or not this class has been destroyed and should not be used.
    private boolean mDestroyed;

    private class InitTask extends BackgroundOnlyAsyncTask<Void> {
        @Override
        protected Void doInBackground() {
            initDiskCache();
            return null;
        }
    }

    private class ClearTask extends BackgroundOnlyAsyncTask<Void> {
        @Override
        protected Void doInBackground() {
            clearDiskCache();
            return null;
        }
    }

    /**
     * Writes to disk cache.
     */
    @VisibleForTesting
    class CacheThumbnailTask extends BackgroundOnlyAsyncTask<Void> {
        private final String mContentId;
        private final Bitmap mBitmap;
        private final int mIconSizePx;

        public CacheThumbnailTask(String contentId, Bitmap bitmap, int iconSizePx) {
            mContentId = contentId;
            mBitmap = bitmap;
            mIconSizePx = iconSizePx;
        }

        @Override
        protected Void doInBackground() {
            addToDisk(mContentId, mBitmap, mIconSizePx);
            return null;
        }
    }

    /**
     * Reads from disk cache. If missing, fetch from {@link ThumbnailGenerator}.
     */
    private class GetThumbnailTask extends AsyncTask<Bitmap> {
        private final ThumbnailProvider.ThumbnailRequest mRequest;

        public GetThumbnailTask(ThumbnailProvider.ThumbnailRequest request) {
            mRequest = request;
        }

        @Override
        protected Bitmap doInBackground() {
            if (sDiskLruCache.contains(
                        Pair.create(mRequest.getContentId(), mRequest.getIconSize()))) {
                return getFromDisk(mRequest.getContentId(), mRequest.getIconSize());
            }
            return null;
        }

        @Override
        protected void onPostExecute(Bitmap bitmap) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.ThumbnailDiskStorage.CachedBitmap.Found", bitmap != null);

            if (bitmap != null) {
                onThumbnailRetrieved(mRequest.getContentId(), bitmap, mRequest.getIconSize());
                return;
            }
            // Asynchronously process the file to make a thumbnail.
            mThumbnailGenerator.retrieveThumbnail(mRequest, ThumbnailDiskStorage.this);
        }
    }

    /**
     * Removes thumbnails with the given contentId from disk cache.
     */
    private class RemoveThumbnailTask extends BackgroundOnlyAsyncTask<Void> {
        private final String mContentId;

        public RemoveThumbnailTask(String contentId) {
            mContentId = contentId;
        }

        @Override
        protected Void doInBackground() {
            // Check again if thumbnails with the specified content ID still exists
            if (!sIconSizesMap.containsKey(mContentId)) return null;

            // Create a copy of the set of icon sizes because they can't be removed from the set
            // while iterating through the set
            ArrayList<Integer> iconSizes = new ArrayList<Integer>(sIconSizesMap.get(mContentId));
            for (int iconSize : iconSizes) {
                removeFromDiskHelper(Pair.create(mContentId, iconSize));
            }
            return null;
        }
    }

    @VisibleForTesting
    ThumbnailDiskStorage(ThumbnailStorageDelegate delegate, ThumbnailGenerator thumbnailGenerator,
            int maxCacheSizeBytes) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
        mThumbnailGenerator = thumbnailGenerator;
        mMaxCacheBytes = maxCacheSizeBytes;
        new InitTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Constructs an instance of ThumbnailDiskStorage with the {@link ThumbnailStorageDelegate} to
     * call back to.
     * @param requester The {@link ThumbnailStorageDelegate} that makes requests.
     * @return An instance of {@link ThumbnailDiskStorage}.
     */
    public static ThumbnailDiskStorage create(ThumbnailStorageDelegate delegate) {
        return new ThumbnailDiskStorage(delegate, new ThumbnailGenerator(), MAX_CACHE_BYTES);
    }

    /**
     * Destroys the {@link ThumbnailGenerator}.
     */
    public void destroy() {
        mThumbnailGenerator.destroy();
        mDestroyed = true;
    }

    /**
     * Clears all cached files.
     */
    public void clear() {
        ThreadUtils.assertOnUiThread();
        new ClearTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Retrieves the requested thumbnail.
     * @param request The request for the thumbnail
     */
    public void retrieveThumbnail(ThumbnailProvider.ThumbnailRequest request) {
        ThreadUtils.assertOnUiThread();
        if (mDestroyed || TextUtils.isEmpty(request.getContentId())) return;

        new GetThumbnailTask(request).executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Called when thumbnail is ready, either retrieved from disk or generated by
     * {@link ThumbnailGenerator}.
     * @param request The request for the thumbnail.
     * @param bitmap The thumbnail requested.
     * @param iconSizePx Requested size (maximum required dimension of the smaller side) of the
     * thumbnail requested.
     */
    @Override
    public void onThumbnailRetrieved(
            @NonNull String contentId, @Nullable Bitmap bitmap, int iconSizePx) {
        // If we've been destroyed, drop any responses coming back from retrieval tasks.
        if (mDestroyed) return;

        ThreadUtils.assertOnUiThread();
        if (bitmap != null && !TextUtils.isEmpty(contentId)) {
            new CacheThumbnailTask(contentId, bitmap, iconSizePx)
                    .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        }
        mDelegate.onThumbnailRetrieved(contentId, bitmap);
    }

    /**
     * Read previously cached thumbnail-related info from disk. Initialize only once. Invoked on
     * background thread.
     */
    @VisibleForTesting
    void initDiskCache() {
        if (isInitialized()) return;

        ThreadUtils.assertOnBackgroundThread();
        mDirectory = getDiskCacheDir(ContextUtils.getApplicationContext(), "thumbnails");
        if (!mDirectory.exists()) {
            boolean dirCreated = false;
            try {
                dirCreated = mDirectory.mkdir();
            } catch (SecurityException se) {
                Log.e(TAG, "Error while creating thumbnails directory.", se);
            }
            if (!dirCreated) return;
        }
        File[] cachedFiles = mDirectory.listFiles();
        if (cachedFiles == null) return;

        for (File file : cachedFiles) {
            AtomicFile atomicFile = new AtomicFile(file);
            try {
                ThumbnailEntry entry = ThumbnailEntry.parseFrom(atomicFile.readFully());
                if (!entry.hasContentId()) continue;

                String contentId = entry.getContentId().getId();
                if (!entry.hasSizePx()) continue;

                int iconSizePx = entry.getSizePx();

                // Update internal cache state.
                sDiskLruCache.add(Pair.create(contentId, iconSizePx));
                if (sIconSizesMap.containsKey(contentId)) {
                    sIconSizesMap.get(contentId).add(iconSizePx);
                } else {
                    HashSet<Integer> iconSizes = new HashSet<Integer>();
                    iconSizes.add(iconSizePx);
                    sIconSizesMap.put(contentId, iconSizes);
                }
                mSizeBytes += file.length();
            } catch (IOException e) {
                Log.e(TAG, "Error while reading from disk.", e);
            }
        }

        RecordHistogram.recordMemoryKBHistogram("Android.ThumbnailDiskStorage.Size",
                (int) (mSizeBytes / ConversionUtils.BYTES_PER_KILOBYTE));
    }

    /**
     * Adds thumbnail to disk as most recent. Thumbnail with an existing content ID in cache will be
     * replaced by the newly added. Invoked on background thread.
     * @param contentId Content ID for the thumbnail to cache to disk.
     * @param bitmap The thumbnail to cache.
     * @param iconSizePx Requested size (maximum required dimension (pixel) of the smaller side) of
     * the thumbnail.
     *
     * TODO(angelashao): Use a DB to store thumbnail-related data. (crbug.com/747555)
     */
    @VisibleForTesting
    void addToDisk(String contentId, Bitmap bitmap, int iconSizePx) {
        ThreadUtils.assertOnBackgroundThread();
        if (!isInitialized()) return;

        if (sDiskLruCache.contains(Pair.create(contentId, iconSizePx))) {
            removeFromDiskHelper(Pair.create(contentId, iconSizePx));
        }

        FileOutputStream fos = null;
        AtomicFile atomicFile = null;
        try {
            // Compress bitmap to PNG.
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, baos);
            byte[] compressedBitmapBytes = baos.toByteArray();

            // Construct proto.
            ThumbnailEntry newEntry =
                    ThumbnailEntry.newBuilder()
                            .setContentId(ContentId.newBuilder().setId(contentId))
                            .setSizePx(iconSizePx)
                            .setCompressedPng(ByteString.copyFrom(compressedBitmapBytes))
                            .build();

            // Write proto to disk.
            File newFile = new File(getThumbnailFilePath(contentId, iconSizePx));
            atomicFile = new AtomicFile(newFile);
            fos = atomicFile.startWrite();
            fos.write(newEntry.toByteArray());
            atomicFile.finishWrite(fos);

            // Update internal cache state.
            sDiskLruCache.add(Pair.create(contentId, iconSizePx));
            if (sIconSizesMap.containsKey(contentId)) {
                sIconSizesMap.get(contentId).add(iconSizePx);
            } else {
                HashSet<Integer> iconSizes = new HashSet<Integer>();
                iconSizes.add(iconSizePx);
                sIconSizesMap.put(contentId, iconSizes);
            }
            mSizeBytes += newFile.length();

            trim();
        } catch (IOException e) {
            Log.e(TAG, "Error while writing to disk.", e);
            atomicFile.failWrite(fos);
        }
    }

    private boolean isInitialized() {
        return mDirectory != null;
    }

    /**
     * Retrieves bitmap with {@code contentId} and {@code iconSizePx} from cache. Invoked on
     * background thread.
     * @param contentId The content ID of the requested thumbnail.
     * @param iconSizePx Requested size (maximum required dimension (pixel) of the smaller side) of
     * the requested thumbnail.
     * @return Bitmap If thumbnail is not cached to disk, this is null.
     */
    @VisibleForTesting
    @Nullable
    Bitmap getFromDisk(String contentId, int iconSizePx) {
        ThreadUtils.assertOnBackgroundThread();
        if (!isInitialized()) return null;

        if (!sDiskLruCache.contains(Pair.create(contentId, iconSizePx))) return null;

        Bitmap bitmap = null;
        FileInputStream fis = null;
        try {
            String thumbnailFilePath = getThumbnailFilePath(contentId, iconSizePx);
            File file = new File(thumbnailFilePath);
            // If file doesn't exist, {@link mSizeBytes} cannot be updated to account for the
            // removal but this is fine in the long-run when trim happens.
            if (!file.exists()) return null;

            AtomicFile atomicFile = new AtomicFile(file);
            fis = atomicFile.openRead();
            ThumbnailEntry entry = ThumbnailEntry.parseFrom(atomicFile.readFully());
            if (!entry.hasCompressedPng()) return null;

            bitmap = BitmapFactory.decodeByteArray(
                    entry.getCompressedPng().toByteArray(), 0, entry.getCompressedPng().size());
        } catch (IOException e) {
            Log.e(TAG, "Error while reading from disk.", e);
        } finally {
            StreamUtil.closeQuietly(fis);
        }

        return bitmap;
    }

    /**
     * Trim the cache to stay under the max cache size by removing the oldest entries.
     */
    @VisibleForTesting
    void trim() {
        ThreadUtils.assertOnBackgroundThread();
        while (mSizeBytes > mMaxCacheBytes) {
            removeFromDiskHelper(sDiskLruCache.iterator().next());
        }
    }

    /**
     * Clear all files in the disk cache.
     */
    @VisibleForTesting
    void clearDiskCache() {
        ThreadUtils.assertOnBackgroundThread();
        while (mSizeBytes > 0) {
            removeFromDiskHelper(sDiskLruCache.iterator().next());
        }
    }

    /**
     * Remove thumbnail identified by {@code contentIdSizePair}. If out of sync with disk, rely on
     * restart or trim (in the long-run) to be in sync.
     * @param contentIdSizePair Pair of the content ID and requested size (maximum required
     * dimension of the smaller side) of the thumbnail to remove.
     */
    @VisibleForTesting
    void removeFromDiskHelper(Pair<String, Integer> contentIdSizePair) {
        ThreadUtils.assertOnBackgroundThread();

        String contentId = contentIdSizePair.first;
        int iconSizePx = contentIdSizePair.second;
        File file = new File(getThumbnailFilePath(contentId, iconSizePx));
        if (!file.exists()) {
            Log.e(TAG, "Error while removing from disk. File does not exist.");
            return;
        }

        long fileSizeBytes = 0;
        try {
            fileSizeBytes = file.length();
        } catch (SecurityException se) {
            Log.e(TAG, "Error while removing from disk. File denied read access.", se);
        }
        AtomicFile atomicFile = new AtomicFile(file);
        atomicFile.delete();

        // Update internal cache state.
        sDiskLruCache.remove(contentIdSizePair);
        sIconSizesMap.get(contentId).remove(iconSizePx);
        if (sIconSizesMap.get(contentId).size() == 0) {
            sIconSizesMap.remove(contentId);
        }
        mSizeBytes -= fileSizeBytes;
    }

    /**
     * Remove thumbnails with the {@code contentId} from disk.
     * @param contentId Content ID for the thumbnail.
     */
    public void removeFromDisk(String contentId) {
        ThreadUtils.assertOnUiThread();
        if (!isInitialized()) return;

        if (!sIconSizesMap.containsKey(contentId)) return;

        new RemoveThumbnailTask(contentId).executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Gets the file path for a thumbnail with the given content ID and size.
     * @param contentId Content ID for the thumbnail.
     * @param iconSizePx Requested size (maximum dimension (pixel) of the smaller side) of
     * thumbnail.
     * @return File path.
     */
    private String getThumbnailFilePath(String contentId, int iconSizePx) {
        return mDirectory.getPath() + File.separator + contentId + iconSizePx + ".entry";
    }

    /**
     * Get directory for thumbnail entries in the designated app (internal) cache directory.
     * The directory's name must be unique.
     * @param context The application's context.
     * @return The path to the thumbnail cache directory.
     */
    private static File getDiskCacheDir(Context context, String thumbnailDirName) {
        return new File(context.getCacheDir().getPath() + File.separator + thumbnailDirName);
    }
}
