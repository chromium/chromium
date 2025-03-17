// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.collection.ArraySet;
import androidx.collection.LruCache;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.BitmapCache;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedHashMap;
import java.util.Locale;

/**
 * Concrete implementation of {@link ThumbnailProvider}.
 *
 * <p>Thumbnails are cached in {@link BitmapCache}. The cache key is a pair of the filepath and the
 * height/width of the thumbnail. Value is the thumbnail.
 *
 * <p>A map is used to store the mapping between contentID, to allow multiple requests to be
 * processed in parallel.
 *
 * <p>without duplicating work to decode the same image for two different requests.
 */
@NullMarked
public class ThumbnailProviderImpl implements ThumbnailProvider, ThumbnailStorageDelegate {
    @IntDef({ClientType.DOWNLOAD_HOME, ClientType.NTP_SUGGESTIONS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClientType {
        int DOWNLOAD_HOME = 0;
        int NTP_SUGGESTIONS = 1;
    }

    /**
     * Avoid excessive requests leading to no available threads, add limitation. Since if the number
     * of running threads is too large and all threads are busy with no idle threads available,
     * `DCHECK(worker_to_wakeup)` in `EnsureEnoughWorkersLockRequired` will fail. This happens
     * because `idle_workers_set_.Take()` cannot retrieve an idle thread from the idle worker set
     * and therefore returns `nullptr`.
     */
    private static final int MAX_REQUEST_LIMIT = 8;

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
    private final BitmapCache mBitmapCache;

    /** The client type of the client using this provider. */
    private final @ClientType int mClient;

    /**
     * Tracks a set of Content Ids where thumbnail generation or retrieval failed. This should
     * prevent making subsequent (potentially expensive) thumbnail generation requests when there
     * would be no point.
     */
    private final LruCache<String /* Content Id */, Object /* Placeholder */> mNoBitmapCache =
            new LruCache<>(100);

    /** Map storing waiting requests in the queue */
    private final LinkedHashMap<String /* Content Id */, ThumbnailRequest> mRequestQueue =
            new LinkedHashMap<>();

    /** Map storing in progress requests */
    private final LinkedHashMap<String /* getKey(contentId,iconSize) */, ArraySet<ThumbnailRequest>>
            mInProgressRequestMap = new LinkedHashMap<>();

    private final ThumbnailDiskStorage mStorage;

    private int mCacheSizeMaxBytesUma;

    /** The maximum number of concurrent requests to allow. */
    private final int mRequestLimit;

    /**
     * Constructor to build the thumbnail provider with default thumbnail cache size.
     *
     * @param referencePool The application's reference pool.
     * @param client The associated client type.
     * @param useMultiRequests Whether to use multiple requests or a single request for fetching
     *     thumbnails.
     */
    public ThumbnailProviderImpl(
            DiscardableReferencePool referencePool,
            @ClientType int client,
            boolean useMultiRequests) {
        this(referencePool, DEFAULT_MAX_CACHE_BYTES, client, useMultiRequests);
    }

    /**
     * Constructor to build the thumbnail provider.
     *
     * @param referencePool The application's reference pool.
     * @param bitmapCacheSizeByte The size in bytes of the in-memory LRU bitmap cache.
     * @param client The associated client type.
     * @param useMultiRequests Whether to use multiple requests or a single request for fetching
     *     thumbnails.
     */
    public ThumbnailProviderImpl(
            DiscardableReferencePool referencePool,
            int bitmapCacheSizeByte,
            @ClientType int client,
            boolean useMultiRequests) {
        ThreadUtils.assertOnUiThread();
        mBitmapCache = new BitmapCache(referencePool, bitmapCacheSizeByte);
        mStorage = ThumbnailDiskStorage.create(this);
        mClient = client;
        mRequestLimit = useMultiRequests ? MAX_REQUEST_LIMIT : 1;
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        // Drop any references to any current requests.
        mRequestQueue.clear();
        mInProgressRequestMap.clear();
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

        String contentId = request.getContentId();
        if (TextUtils.isEmpty(contentId)) {
            return;
        }

        String requestKey = getKey(contentId, request.getIconSize());
        var inProgressRequests = mInProgressRequestMap.get(requestKey);
        if (inProgressRequests != null) {
            // The request for the same content is already in progress.
            inProgressRequests.add(request);
            return;
        }

        if (mNoBitmapCache.get(contentId) != null) {
            request.onThumbnailRetrieved(contentId, null);
            return;
        }

        Bitmap cachedBitmap = getBitmapFromCache(requestKey);
        if (cachedBitmap != null) {
            request.onThumbnailRetrieved(contentId, cachedBitmap);
            return;
        }

        mRequestQueue.put(contentId, request);
        processQueue();
    }

    /** Removes a particular file from the pending queue. */
    @Override
    public void cancelRetrieval(ThumbnailRequest request) {
        ThreadUtils.assertOnUiThread();
        // Will only cancel those in the waiting list. Canceling does nothing for those in progress.
        // Because in-progress tasks are already in the pool. Removing keys from the in progress
        // map helps nothing.
        String requestKey = getKey(assumeNonNull(request.getContentId()), request.getIconSize());
        if (!mInProgressRequestMap.containsKey(requestKey)) {
            // Only remove the request from the map if it is not in progress.
            mRequestQueue.remove(request.getContentId());
        }
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
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::processNextRequest);
    }

    private String getKey(String contentId, int bitmapSizePx) {
        return String.format(Locale.US, "id=%s, size=%d", contentId, bitmapSizePx);
    }

    private @Nullable Bitmap getBitmapFromCache(String requestKey) {
        Bitmap cachedBitmap = mBitmapCache.getBitmap(requestKey);
        assert cachedBitmap == null || !cachedBitmap.isRecycled();

        RecordHistogram.recordBooleanHistogram(
                "Android.ThumbnailProvider.CachedBitmap.Found." + getClientTypeUmaSuffix(mClient),
                cachedBitmap != null);
        return cachedBitmap;
    }

    private void processNextRequest() {
        ThreadUtils.assertOnUiThread();

        if (mRequestQueue.isEmpty()) return;

        // peek first request in queue
        var firstEntry = mRequestQueue.entrySet().iterator().next();
        String contentId = firstEntry.getKey();
        ThumbnailRequest currentRequest = firstEntry.getValue();

        // avoid nullable after destroy()
        if (currentRequest == null) return;

        String requestKey = getKey(contentId, currentRequest.getIconSize());
        Bitmap cachedBitmap = getBitmapFromCache(requestKey);
        if (cachedBitmap == null) {
            if (mInProgressRequestMap.size() >= mRequestLimit) return;
            mRequestQueue.remove(contentId);
            if (!mInProgressRequestMap.containsKey(requestKey)) {
                mInProgressRequestMap.put(requestKey, new ArraySet<>());
            }
            mInProgressRequestMap.get(requestKey).add(currentRequest);
            handleCacheMiss(currentRequest);
        } else {
            // Send back the already-processed file.
            onThumbnailRetrieved(contentId, cachedBitmap, currentRequest.getIconSize());
        }
    }

    /**
     * In the event of a cache miss from the in-memory cache, the thumbnail request is routed to one
     * of the following :
     *
     * <pre>
     * 1. May be the thumbnail request can directly provide the thumbnail.
     * 2. Otherwise, the request is sent to {@link ThumbnailDiskStorage} which is a disk cache. If
     * not found in disk cache, it would request the {@link ThumbnailGenerator} to generate a new
     * thumbnail for the given file path.
     * </pre>
     *
     * @param request Parameters that describe the thumbnail being retrieved
     */
    private void handleCacheMiss(ThumbnailProvider.ThumbnailRequest request) {
        boolean providedByThumbnailRequest =
                request.getThumbnail(
                        bitmap ->
                                onThumbnailRetrieved(
                                        assumeNonNull(request.getContentId()),
                                        bitmap,
                                        request.getIconSize()));

        if (!providedByThumbnailRequest) {
            // Asynchronously process the file to make a thumbnail.
            assert !TextUtils.isEmpty(request.getFilePath());
            mStorage.retrieveThumbnail(request);
        }
    }

    /**
     * Called when thumbnail is ready, retrieved from memory cache or by {@link
     * ThumbnailDiskStorage} or by {@link ThumbnailRequest#getThumbnail}.
     *
     * @param contentId Content ID for the thumbnail retrieved.
     * @param bitmap The thumbnail retrieved.
     * @param iconSizePx Icon size for the thumbnail retrieved.
     */
    @Override
    public void onThumbnailRetrieved(String contentId, @Nullable Bitmap bitmap, int iconSizePx) {
        ThreadUtils.assertOnUiThread();

        ArraySet<ThumbnailRequest> currentRequestSet = new ArraySet<>();

        String requestKey = getKey(contentId, iconSizePx);
        var inProcessRequests = mInProgressRequestMap.remove(requestKey);
        if (inProcessRequests != null) {
            currentRequestSet.addAll(inProcessRequests);
        }

        var pendingRequest = mRequestQueue.remove(contentId);
        if (pendingRequest != null) {
            currentRequestSet.add(pendingRequest);
        }

        if (currentRequestSet.isEmpty()) {
            processQueue();
            return;
        }

        ThumbnailRequest retrievedRequest = currentRequestSet.valueAt(0);
        assert retrievedRequest != null;
        if (bitmap != null) {
            // The bitmap returned here is retrieved from the native side. The image decoder there
            // scales down the image (if it is too big) so that one of its sides is smaller than or
            // equal to the required size. We check here that the returned image satisfies this
            // criteria.
            assert Math.min(bitmap.getWidth(), bitmap.getHeight())
                    <= retrievedRequest.getIconSize();

            // We set the key pair to contain the required size (maximum dimension (pixel) of the
            // smaller side) instead of the minimal dimension of the thumbnail so that future
            // fetches of this thumbnail can recognise the key in the cache.
            String key = getKey(contentId, retrievedRequest.getIconSize());
            mBitmapCache.putBitmap(key, bitmap);
            mNoBitmapCache.remove(contentId);
            mCacheSizeMaxBytesUma = Math.max(mCacheSizeMaxBytesUma, mBitmapCache.size());
            for (ThumbnailRequest request : currentRequestSet) {
                request.onThumbnailRetrieved(contentId, bitmap);
            }
        } else {
            mNoBitmapCache.put(contentId, NO_BITMAP_PLACEHOLDER);
            for (ThumbnailRequest request : currentRequestSet) {
                request.onThumbnailRetrieved(contentId, null);
            }
        }
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
