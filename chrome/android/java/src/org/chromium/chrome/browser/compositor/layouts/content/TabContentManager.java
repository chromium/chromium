// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import static java.lang.Math.min;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.native_page.FrozenNativePage;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.usage_stats.SuspendedTab;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroid;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * The TabContentManager is responsible for serving tab contents to the UI components. Contents
 * could be live or static thumbnails.
 */
@JNINamespace("android")
public class TabContentManager {
    // These are used for UMA logging, so append only. Please update the
    // GridTabSwitcherThumbnailFetchingResult enum in enums.xml if these change.
    @IntDef({ThumbnailFetchingResult.GOT_JPEG, ThumbnailFetchingResult.GOT_ETC1,
            ThumbnailFetchingResult.GOT_NOTHING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ThumbnailFetchingResult {
        int GOT_JPEG = 0;
        int GOT_ETC1 = 1;
        int GOT_NOTHING = 2;
        int NUM_ENTRIES = 3;
    }
    private static final String UMA_THUMBNAIL_FETCHING_RESULT =
            "GridTabSwitcher.ThumbnailFetchingResult";

    private final float mThumbnailScale;
    private final int mFullResThumbnailsMaxSize;
    private final ContentOffsetProvider mContentOffsetProvider;
    private int[] mPriorityTabIds;
    private long mNativeTabContentManager;

    private final ArrayList<ThumbnailChangeListener> mListeners =
            new ArrayList<ThumbnailChangeListener>();

    private boolean mSnapshotsEnabled;

    /**
     * The Java interface for listening to thumbnail changes.
     */
    public interface ThumbnailChangeListener {
        /**
         * @param id The tab id.
         */
        public void onThumbnailChange(int id);
    }

    /**
     * @param context               The context that this cache is created in.
     * @param resourceId            The resource that this value might be defined in.
     * @param commandLineSwitch     The switch for which we would like to extract value from.
     * @return the value of an integer resource.  If the value is overridden on the command line
     * with the given switch, return the override instead.
     */
    private static int getIntegerResourceWithOverride(Context context, int resourceId,
            String commandLineSwitch) {
        int val = -1;
        // TODO(crbug/959054): Convert this to Finch config.
        if (FeatureUtilities.isGridTabSwitcherEnabled()) {
            // With Grid Tab Switcher, we can greatly reduce the capacity of thumbnail cache.
            // See crbug.com/959054 for more details.
            if (resourceId == R.integer.default_thumbnail_cache_size) val = 2;
            if (resourceId == R.integer.default_approximation_thumbnail_cache_size) val = 8;
            assert val != -1;
        } else {
            val = context.getResources().getInteger(resourceId);
        }
        String switchCount = CommandLine.getInstance().getSwitchValue(commandLineSwitch);
        if (switchCount != null) {
            int count = Integer.parseInt(switchCount);
            val = count;
        }
        return val;
    }

    /**
     * @param context               The context that this cache is created in.
     * @param contentOffsetProvider The provider of content parameter.
     */
    public TabContentManager(Context context, ContentOffsetProvider contentOffsetProvider,
            boolean snapshotsEnabled) {
        mContentOffsetProvider = contentOffsetProvider;
        mSnapshotsEnabled = snapshotsEnabled;

        // Override the cache size on the command line with --thumbnails=100
        int defaultCacheSize = getIntegerResourceWithOverride(
                context, R.integer.default_thumbnail_cache_size, ChromeSwitches.THUMBNAILS);

        mFullResThumbnailsMaxSize = defaultCacheSize;

        int compressionQueueMaxSize =
                context.getResources().getInteger(R.integer.default_compression_queue_size);
        int writeQueueMaxSize =
                context.getResources().getInteger(R.integer.default_write_queue_size);

        // Override the cache size on the command line with
        // --approximation-thumbnails=100
        int approximationCacheSize = getIntegerResourceWithOverride(context,
                R.integer.default_approximation_thumbnail_cache_size,
                ChromeSwitches.APPROXIMATION_THUMBNAILS);

        float thumbnailScale = 1.f;
        boolean useApproximationThumbnails;
        boolean saveJpegThumbnails = FeatureUtilities.isGridTabSwitcherEnabled();
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        float deviceDensity = display.getDipScale();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            // Scale all tablets to MDPI.
            thumbnailScale = 1.f / deviceDensity;
            useApproximationThumbnails = false;
        } else {
            // For phones, reduce the amount of memory usage by capturing a lower-res thumbnail for
            // devices with resolution higher than HDPI (crbug.com/357740).
            if (deviceDensity > 1.5f) {
                thumbnailScale = 1.5f / deviceDensity;
            }
            useApproximationThumbnails = true;
        }
        mThumbnailScale = thumbnailScale;

        mPriorityTabIds = new int[mFullResThumbnailsMaxSize];

        mNativeTabContentManager = TabContentManagerJni.get().init(TabContentManager.this,
                defaultCacheSize, approximationCacheSize, compressionQueueMaxSize,
                writeQueueMaxSize, useApproximationThumbnails, saveJpegThumbnails);
    }

    /**
     * Destroy the native component.
     */
    public void destroy() {
        if (mNativeTabContentManager != 0) {
            TabContentManagerJni.get().destroy(mNativeTabContentManager);
            mNativeTabContentManager = 0;
        }
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeTabContentManager;
    }

    /**
     * Attach the given Tab's cc layer to this {@link TabContentManager}.
     * @param tab Tab whose cc layer will be attached.
     */
    public void attachTab(Tab tab) {
        if (mNativeTabContentManager == 0) return;
        TabContentManagerJni.get().attachTab(
                mNativeTabContentManager, TabContentManager.this, tab, tab.getId());
    }

    /**
     * Detach the given Tab's cc layer from this {@link TabContentManager}.
     * @param tab Tab whose cc layer will be detached.
     */
    public void detachTab(Tab tab) {
        if (mNativeTabContentManager == 0) return;
        TabContentManagerJni.get().detachTab(
                mNativeTabContentManager, TabContentManager.this, tab, tab.getId());
    }

    /**
     * Add a listener to thumbnail changes.
     * @param listener The listener of thumbnail change events.
     */
    public void addThumbnailChangeListener(ThumbnailChangeListener listener) {
        if (!mListeners.contains(listener)) {
            mListeners.add(listener);
        }
    }

    /**
     * Remove a listener to thumbnail changes.
     * @param listener The listener of thumbnail change events.
     */
    public void removeThumbnailChangeListener(ThumbnailChangeListener listener) {
        mListeners.remove(listener);
    }

    private Bitmap readbackNativeBitmap(final Tab tab, float scale) {
        NativePage nativePage = tab.getNativePage();
        boolean isNativeViewShowing = isNativeViewShowing(tab);
        if (nativePage == null && !isNativeViewShowing) {
            return null;
        }

        View viewToDraw = null;
        if (isNativeViewShowing) {
            viewToDraw = tab.getContentView();
        } else if (!(nativePage instanceof FrozenNativePage)) {
            viewToDraw = nativePage.getView();
        }
        if (viewToDraw == null || viewToDraw.getWidth() == 0 || viewToDraw.getHeight() == 0) {
            return null;
        }

        if (nativePage != null && nativePage instanceof InvalidationAwareThumbnailProvider) {
            if (!((InvalidationAwareThumbnailProvider) nativePage).shouldCaptureThumbnail()) {
                return null;
            }
        }

        return readbackNativeView(viewToDraw, scale, nativePage);
    }

    private Bitmap readbackNativeView(View viewToDraw, float scale, NativePage nativePage) {
        Bitmap bitmap = null;
        float overlayTranslateY = mContentOffsetProvider.getOverlayTranslateY();

        float leftMargin = 0.f;
        float topMargin = 0.f;
        if (viewToDraw.getLayoutParams() instanceof MarginLayoutParams) {
            MarginLayoutParams params = (MarginLayoutParams) viewToDraw.getLayoutParams();
            leftMargin = params.leftMargin;
            topMargin = params.topMargin;
        }

        try {
            bitmap = Bitmap.createBitmap(
                    (int) ((viewToDraw.getMeasuredWidth() + leftMargin) * mThumbnailScale),
                    (int) ((viewToDraw.getMeasuredHeight() + topMargin - overlayTranslateY)
                            * mThumbnailScale),
                    Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError ex) {
            return null;
        }

        Canvas c = new Canvas(bitmap);
        c.scale(scale, scale);
        c.translate(leftMargin, -overlayTranslateY + topMargin);
        if (nativePage != null && nativePage instanceof InvalidationAwareThumbnailProvider) {
            ((InvalidationAwareThumbnailProvider) nativePage).captureThumbnail(c);
        } else {
            viewToDraw.draw(c);
        }

        return bitmap;
    }

    /**
     * @param tabId The id of the {@link Tab} to check for a full sized thumbnail of.
     * @return      Whether or not there is a full sized cached thumbnail for the {@link Tab}
     *              identified by {@code tabId}.
     */
    public boolean hasFullCachedThumbnail(int tabId) {
        if (mNativeTabContentManager == 0) return false;
        return TabContentManagerJni.get().hasFullCachedThumbnail(
                mNativeTabContentManager, TabContentManager.this, tabId);
    }

    /**
     * Call to get a thumbnail for a given tab through a {@link Callback}. If there is
     * no up-to-date thumbnail on disk for the given tab, callback returns null.
     * @param tab The tab to get the thumbnail for.
     * @param callback The callback to send the {@link Bitmap} with. Can be called up to twice when
     *                 {@code forceUpdate}; otherwise always called exactly once.
     * @param forceUpdate Whether to obtain the thumbnail from the live content.
     * @param writeBack When {@code forceUpdate}, whether to write the thumbnail to cache.
     */
    public void getTabThumbnailWithCallback(@NonNull Tab tab, @NonNull Callback<Bitmap> callback,
            boolean forceUpdate, boolean writeBack) {
        if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;

        if (!forceUpdate) {
            assert !writeBack : "writeBack is ignored if not forceUpdate";
            getTabThumbnailFromDisk(tab, callback);
            return;
        }

        // Reading thumbnail from disk is faster than taking screenshot from live Tab, so fetch
        // that first even if |forceUpdate|.
        getTabThumbnailFromDisk(tab, (diskBitmap) -> {
            if (diskBitmap != null) callback.onResult(diskBitmap);

            captureThumbnail(tab, writeBack, (bitmap) -> {
                // Null check to avoid having a Bitmap from getTabThumbnailFromDisk() but
                // cleared here.
                // If invalidation is not needed, readbackNativeBitmap() might not do anything and
                // send back null.
                if (bitmap != null) {
                    callback.onResult(bitmap);
                }
            });
        });
    }

    /**
     * @param tab The {@link Tab} the thumbnail is for.
     * @return The file storing the thumbnail in ETC1 format of a certain {@link Tab}.
     */
    public static File getTabThumbnailFileEtc1(Tab tab) {
        return new File(PathUtils.getThumbnailCacheDirectory(), String.valueOf(tab.getId()));
    }

    /**
     * @param tab The {@link Tab} the thumbnail is for.
     * @return The file storing the thumbnail in JPEG format of a certain {@link Tab}.
     */
    public static File getTabThumbnailFileJpeg(Tab tab) {
        return new File(PathUtils.getThumbnailCacheDirectory(), tab.getId() + ".jpeg");
    }

    private void getTabThumbnailFromDisk(@NonNull Tab tab, @NonNull Callback<Bitmap> callback) {
        // Try JPEG thumbnail first before using the more costly
        // TabContentManagerJni.get().getEtc1TabThumbnail.
        TraceEvent.startAsync("GetTabThumbnailFromDisk", tab.getId());
        new AsyncTask<Bitmap>() {
            @Override
            public Bitmap doInBackground() {
                File file = getTabThumbnailFileJpeg(tab);
                if (!file.isFile()) return null;
                return BitmapFactory.decodeFile(file.getPath());
            }

            @Override
            public void onPostExecute(Bitmap jpeg) {
                TraceEvent.finishAsync("GetTabThumbnailFromDisk", tab.getId());
                if (jpeg != null) {
                    RecordHistogram.recordEnumeratedHistogram(UMA_THUMBNAIL_FETCHING_RESULT,
                            ThumbnailFetchingResult.GOT_JPEG, ThumbnailFetchingResult.NUM_ENTRIES);
                    callback.onResult(jpeg);
                    return;
                }
                if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;
                TabContentManagerJni.get().getEtc1TabThumbnail(
                        mNativeTabContentManager, TabContentManager.this, tab.getId(), (etc1) -> {
                            if (etc1 != null) {
                                RecordHistogram.recordEnumeratedHistogram(
                                        UMA_THUMBNAIL_FETCHING_RESULT,
                                        ThumbnailFetchingResult.GOT_ETC1,
                                        ThumbnailFetchingResult.NUM_ENTRIES);
                            } else {
                                RecordHistogram.recordEnumeratedHistogram(
                                        UMA_THUMBNAIL_FETCHING_RESULT,
                                        ThumbnailFetchingResult.GOT_NOTHING,
                                        ThumbnailFetchingResult.NUM_ENTRIES);
                            }
                            callback.onResult(etc1);
                        });
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Cache the content of a tab as a thumbnail.
     * @param tab The tab whose content we will cache.
     */
    public void cacheTabThumbnail(@NonNull final Tab tab) {
        if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;

        captureThumbnail(tab, true, null);
    }

    private Bitmap cacheNativeTabThumbnail(final Tab tab) {
        assert tab.getNativePage() != null || isNativeViewShowing(tab);

        Bitmap nativeBitmap = readbackNativeBitmap(tab, mThumbnailScale);
        if (nativeBitmap == null) return null;
        TabContentManagerJni.get().cacheTabWithBitmap(mNativeTabContentManager,
                TabContentManager.this, tab, nativeBitmap, mThumbnailScale);
        return nativeBitmap;
    }

    /**
     * Capture the content of a tab as a thumbnail.
     * @param tab The tab whose content we will capture.
     * @param writeToCache Whether write the captured thumbnail to cache. If not, a downsampled
     *                     thumbnail is captured instead.
     * @param callback The callback to send the {@link Bitmap} with.
     */
    private void captureThumbnail(
            @NonNull final Tab tab, boolean writeToCache, @Nullable Callback<Bitmap> callback) {
        assert mNativeTabContentManager != 0;
        assert mSnapshotsEnabled;

        if (tab.getNativePage() != null || isNativeViewShowing(tab)) {
            final float downsamplingScale = 0.5f;
            // If we use readbackNativeBitmap() with a downsampled scale and not saving it through
            // TabContentManagerJni.get().cacheTabWithBitmap( TabContentManager.this, ), the logic
            // of InvalidationAwareThumbnailProvider might prevent captureThumbnail() from getting
            // the latest thumbnail. Therefore, we have to also call cacheNativeTabThumbnail(), and
            // do the downsampling here ourselves. This is less efficient than capturing a
            // downsampled bitmap, but the performance here is not the bottleneck.
            Bitmap bitmap = cacheNativeTabThumbnail(tab);
            if (callback == null) return;
            if (bitmap == null) {
                callback.onResult(null);
                return;
            }
            // In portrait mode, we want to show thumbnails in squares.
            // Therefore, the thumbnail saved in portrait mode needs to be cropped to
            // a square, or it would become too tall and break the layout.
            Matrix matrix = new Matrix();
            matrix.setScale(downsamplingScale, downsamplingScale);
            Bitmap resized = Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(),
                    min(bitmap.getWidth(), bitmap.getHeight()), matrix, true);
            callback.onResult(resized);
        } else {
            if (tab.getWebContents() == null) return;
            // If we don't have to write the thumbnail back to the cache, we can use the faster
            // path of capturing a downsampled copy.
            // This faster path is essential to Tab-to-Grid animation to be smooth.
            final float downsamplingScale = writeToCache ? 1 : 0.5f;
            TabContentManagerJni.get().captureThumbnail(mNativeTabContentManager,
                    TabContentManager.this, tab, mThumbnailScale * downsamplingScale, writeToCache,
                    callback);
        }
    }

    /**
     * Invalidate a thumbnail if the content of the tab has been changed.
     * @param tabId The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateIfChanged(int tabId, String url) {
        if (mNativeTabContentManager != 0) {
            TabContentManagerJni.get().invalidateIfChanged(
                    mNativeTabContentManager, TabContentManager.this, tabId, url);
        }
    }

    /**
     * Invalidate a thumbnail of the tab whose id is |id|.
     * @param id The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateTabThumbnail(int id, String url) {
        invalidateIfChanged(id, url);
    }

    /**
     * Update the priority-ordered list of visible tabs.
     * @param priority The list of tab ids ordered in terms of priority.
     */
    public void updateVisibleIds(List<Integer> priority, int primaryTabId) {
        if (mNativeTabContentManager != 0) {
            int idsSize = min(mFullResThumbnailsMaxSize, priority.size());

            if (idsSize != mPriorityTabIds.length) {
                mPriorityTabIds = new int[idsSize];
            }

            for (int i = 0; i < idsSize; i++) {
                mPriorityTabIds[i] = priority.get(i);
            }
            TabContentManagerJni.get().updateVisibleIds(mNativeTabContentManager,
                    TabContentManager.this, mPriorityTabIds, primaryTabId);
        }
    }

    /**
     * Removes a thumbnail of the tab whose id is |tabId|.
     * @param tabId The Id of the tab whose thumbnail is being removed.
     */
    public void removeTabThumbnail(int tabId) {
        if (mNativeTabContentManager != 0) {
            TabContentManagerJni.get().removeTabThumbnail(
                    mNativeTabContentManager, TabContentManager.this, tabId);
        }
    }

    @VisibleForTesting
    public void setCaptureMinRequestTimeForTesting(int timeMs) {
        TabContentManagerJni.get().setCaptureMinRequestTimeForTesting(
                mNativeTabContentManager, TabContentManager.this, timeMs);
    }

    @VisibleForTesting
    public int getPendingReadbacksForTesting() {
        return TabContentManagerJni.get().getPendingReadbacksForTesting(
                mNativeTabContentManager, TabContentManager.this);
    }

    @CalledByNative
    protected void notifyListenersOfThumbnailChange(int tabId) {
        for (ThumbnailChangeListener listener : mListeners) {
            listener.onThumbnailChange(tabId);
        }
    }

    private boolean isNativeViewShowing(Tab tab) {
        return tab != null && (SadTab.isShowing(tab) || SuspendedTab.isShowing(tab));
    }

    @NativeMethods
    interface Natives {
        // Class Object Methods
        long init(TabContentManager caller, int defaultCacheSize, int approximationCacheSize,
                int compressionQueueMaxSize, int writeQueueMaxSize,
                boolean useApproximationThumbnail, boolean saveJpegThumbnails);

        void attachTab(long nativeTabContentManager, TabContentManager caller, Tab tab, int tabId);
        void detachTab(long nativeTabContentManager, TabContentManager caller, Tab tab, int tabId);
        boolean hasFullCachedThumbnail(
                long nativeTabContentManager, TabContentManager caller, int tabId);
        void captureThumbnail(long nativeTabContentManager, TabContentManager caller, Object tab,
                float thumbnailScale, boolean writeToCache, Callback<Bitmap> callback);
        void cacheTabWithBitmap(long nativeTabContentManager, TabContentManager caller, Object tab,
                Object bitmap, float thumbnailScale);
        void invalidateIfChanged(
                long nativeTabContentManager, TabContentManager caller, int tabId, String url);
        void updateVisibleIds(long nativeTabContentManager, TabContentManager caller,
                int[] priority, int primaryTabId);
        void removeTabThumbnail(long nativeTabContentManager, TabContentManager caller, int tabId);
        void getEtc1TabThumbnail(long nativeTabContentManager, TabContentManager caller, int tabId,
                Callback<Bitmap> callback);
        void setCaptureMinRequestTimeForTesting(
                long nativeTabContentManager, TabContentManager caller, int timeMs);
        int getPendingReadbacksForTesting(long nativeTabContentManager, TabContentManager caller);
        void destroy(long nativeTabContentManager);
    }
}
