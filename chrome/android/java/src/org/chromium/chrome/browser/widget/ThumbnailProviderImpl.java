// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.graphics.Bitmap;
import android.support.v4.util.LruCache;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.util.BitmapCache;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Locale;

/**
 * Concrete implementation of {@link ThumbnailProvider}.
 *
 * Thumbnails are cached in {@link BitmapCache}. The cache key is a pair of the filepath and
 * the height/width of the thumbnail. Value is the thumbnail.
 *
 * A queue of requests is maintained in FIFO order.
 *
 * TODO(dfalcantara): Figure out how to send requests simultaneously to the utility process without
 *                    duplicating work to decode the same image for two different requests.
 */
public class ThumbnailProviderImpl implements ThumbnailProvider, ThumbnailStorageDelegate {
    @IntDef({ClientType.DOWNLOAD_HOME, ClientType.NTP_SUGGESTIONS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClientType {
        int DOWNLOAD_HOME = 0;
        int NTP_SUGGESTIONS = 1;
    }

    /** Default in-memory thumbnail cache size. */
    private static final int DEFAULT_MAX_CACHE_BYTES = 5 * ConversionUtils.BYTES_PER_MEGABYTE;

    /**
     * Helper object to store in the LruCache when we don't really need a value but can't use null.
     */
    private static final Object NO_BITMAP_PLACEHOLDER = new Object();

    /**
     * An in-memory LRU cache used to cache bitmaps, mostly improve performance for scrolling, when
     * the view is recycled and needs a new thumbnail.
     */
    private BitmapCache mBitmapCache;

    /** The client type of the client using this provider. */
    private final @ClientType int mClient;

    /**
     * Tracks a set of Content Ids where thumbnail generation or retrieval failed.  This should
     * prevent making subsequent (potentially expensive) thumbnail generation requests when there
     * would be no point.
     */
    private LruCache<String /* Content Id */, Object /* Placeholder */> mNoBitmapCache =
            new LruCache<>(100);

    /** Queue of files to retrieve thumbnails for. */
    private final Deque<ThumbnailRequest> mRequestQueue = new ArrayDeque<>();

    /** Request that is currently having its thumbnail retrieved. */
    private ThumbnailRequest mCurrentRequest;

    private ThumbnailDiskStorage mStorage;

    private int mCacheSizeMaxBytesUma;

    /**
     * Constructor to build the thumbnail provider with default thumbnail cache size.
     * @param referencePool The application's reference pool.
     * @param client The associated client type.
     */
    public ThumbnailProviderImpl(DiscardableReferencePool referencePool, @ClientType int client) {
        this(referencePool, DEFAULT_MAX_CACHE_BYTES, client);
    }

    /**
     * Constructor to build the thumbnail provider.
     * @param referencePool The application's reference pool.
     * @param bitmapCacheSizeByte The size in bytes of the in-memory LRU bitmap cache.
     * @param client The associated client type.
     */
    public ThumbnailProviderImpl(DiscardableReferencePool referencePool, int bitmapCacheSizeByte,
            @ClientType int client) {
        ThreadUtils.assertOnUiThread();
        mBitmapCache = new BitmapCache(referencePool, bitmapCacheSizeByte);
        mStorage = ThumbnailDiskStorage.create(this);
        mClient = client;
    }

    @Override
    public void destroy() {
        // Drop any references to any current requests.
        mCurrentRequest = null;
        mRequestQueue.clear();

        ThreadUtils.assertOnUiThread();
        recordBitmapCacheSize();
        mStorage.destroy();
        mBitmapCache.destroy();
    }

    /**
     * The returned bitmap will have at least one of its dimensions smaller than or equal to the
     * size specified in the request. Requests with no file path or content ID will not be
     * processed.
     *
     * @param request Parameters that describe the thumbnail being retrieved.
     */
    @Override
    public void getThumbnail(ThumbnailRequest request) {
        ThreadUtils.assertOnUiThread();

        if (TextUtils.isEmpty(request.getContentId())) {
            return;
        }

        if (mNoBitmapCache.get(request.getContentId()) != null) {
            request.onThumbnailRetrieved(request.getContentId(), null);
            return;
        }

        Bitmap cachedBitmap = getBitmapFromCache(request.getContentId(), request.getIconSize());
        if (cachedBitmap != null) {
            request.onThumbnailRetrieved(request.getContentId(), cachedBitmap);
            return;
        }

        mRequestQueue.offer(request);
        processQueue();
    }

    /** Removes a particular file from the pending queue. */
    @Override
    public void cancelRetrieval(ThumbnailRequest request) {
        ThreadUtils.assertOnUiThread();
        if (mRequestQueue.contains(request)) mRequestQueue.remove(request);
    }

    /**
     * Removes the thumbnails (different sizes) with {@code contentId} from disk.
     * @param contentId The content ID of the thumbnail to remove.
     */
    @Override
    public void removeThumbnailsFromDisk(String contentId) {
        mStorage.removeFromDisk(contentId);
    }

    private void processQueue() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::processNextRequest);
    }

    private String getKey(String contentId, int bitmapSizePx) {
        return String.format(Locale.US, "id=%s, size=%d", contentId, bitmapSizePx);
    }

    private Bitmap getBitmapFromCache(String contentId, int bitmapSizePx) {
        String key = getKey(contentId, bitmapSizePx);
        Bitmap cachedBitmap = mBitmapCache.getBitmap(key);
        assert cachedBitmap == null || !cachedBitmap.isRecycled();

        RecordHistogram.recordBooleanHistogram(
                "Android.ThumbnailProvider.CachedBitmap.Found." + getClientTypeUmaSuffix(mClient),
                cachedBitmap != null);
        return cachedBitmap;
    }

    private void processNextRequest() {
        ThreadUtils.assertOnUiThread();
        if (mCurrentRequest != null) return;
        if (mRequestQueue.isEmpty()) return;

        mCurrentRequest = mRequestQueue.poll();

        Bitmap cachedBitmap =
                getBitmapFromCache(mCurrentRequest.getContentId(), mCurrentRequest.getIconSize());
        if (cachedBitmap == null) {
            handleCacheMiss(mCurrentRequest);
        } else {
            // Send back the already-processed file.
            onThumbnailRetrieved(mCurrentRequest.getContentId(), cachedBitmap);
        }
    }

    /**
     * In the event of a cache miss from the in-memory cache, the thumbnail request is routed to one
     * of the following :
     * 1. May be the thumbnail request can directly provide the thumbnail.
     * 2. Otherwise, the request is sent to {@link ThumbnailDiskStorage} which is a disk cache. If
     * not found in disk cache, it would request the {@link ThumbnailGenerator} to generate a new
     * thumbnail for the given file path.
     * @param request Parameters that describe the thumbnail being retrieved
     */
    private void handleCacheMiss(ThumbnailProvider.ThumbnailRequest request) {
        boolean providedByThumbnailRequest = request.getThumbnail(
                bitmap -> onThumbnailRetrieved(request.getContentId(), bitmap));

        if (!providedByThumbnailRequest) {
            // Asynchronously process the file to make a thumbnail.
            assert !TextUtils.isEmpty(request.getFilePath());
            mStorage.retrieveThumbnail(request);
        }
    }

    /**
     * Called when thumbnail is ready, retrieved from memory cache or by
     * {@link ThumbnailDiskStorage} or by {@link ThumbnailRequest#getThumbnail}.
     * @param contentId Content ID for the thumbnail retrieved.
     * @param bitmap The thumbnail retrieved.
     */
    @Override
    public void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap bitmap) {
        // Early-out if we have no actual current request.
        if (mCurrentRequest == null) return;

        if (bitmap != null) {
            // The bitmap returned here is retrieved from the native side. The image decoder there
            // scales down the image (if it is too big) so that one of its sides is smaller than or
            // equal to the required size. We check here that the returned image satisfies this
            // criteria.
            assert Math.min(bitmap.getWidth(), bitmap.getHeight()) <= mCurrentRequest.getIconSize();
            assert TextUtils.equals(mCurrentRequest.getContentId(), contentId);

            // We set the key pair to contain the required size (maximum dimension (pixel) of the
            // smaller side) instead of the minimal dimension of the thumbnail so that future
            // fetches of this thumbnail can recognise the key in the cache.
            String key = getKey(contentId, mCurrentRequest.getIconSize());
            mBitmapCache.putBitmap(key, bitmap);
            mNoBitmapCache.remove(contentId);
            mCurrentRequest.onThumbnailRetrieved(contentId, bitmap);

            mCacheSizeMaxBytesUma = Math.max(mCacheSizeMaxBytesUma, mBitmapCache.size());
        } else {
            mNoBitmapCache.put(contentId, NO_BITMAP_PLACEHOLDER);
            mCurrentRequest.onThumbnailRetrieved(contentId, null);
        }

        mCurrentRequest = null;
        processQueue();
    }

    private void recordBitmapCacheSize() {
        RecordHistogram.recordMemoryKBHistogram(
                "Android.ThumbnailProvider.BitmapCache.Size." + getClientTypeUmaSuffix(mClient),
                mCacheSizeMaxBytesUma / ConversionUtils.BYTES_PER_KILOBYTE);
    }

    private static String getClientTypeUmaSuffix(@ClientType int clientType) {
        switch (clientType) {
            case ClientType.DOWNLOAD_HOME:
                return "DownloadHome";
            case ClientType.NTP_SUGGESTIONS:
                return "NTPSnippets";
            default:
                assert false;
                return "Other";
        }
    }
}
