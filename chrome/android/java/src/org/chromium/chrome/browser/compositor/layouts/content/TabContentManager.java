// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import static java.lang.Math.min;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.util.Size;
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
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.flags.PostNativeFlag;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.url.GURL;

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
    private static PostNativeFlag sThumbnailCacheRefactor =
            new PostNativeFlag(ChromeFeatureList.THUMBNAIL_CACHE_REFACTOR);

    // These are used for UMA logging, so append only. Please update the
    // GridTabSwitcherThumbnailFetchingResult enum in enums.xml if these change.
    @IntDef({ThumbnailFetchingResult.GOT_JPEG, ThumbnailFetchingResult.GOT_ETC1,
            ThumbnailFetchingResult.GOT_NOTHING,
            ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ThumbnailFetchingResult {
        int GOT_JPEG = 0;
        int GOT_ETC1 = 1;
        int GOT_NOTHING = 2;
        int GOT_DIFFERENT_ASPECT_RATIO_JPEG = 3;
        int NUM_ENTRIES = 4;
    }

    // This is to accommodate for pixel rounding errors.
    public static final double PIXEL_TOLERANCE_PERCENT = 0.02;

    @VisibleForTesting
    public static final String UMA_THUMBNAIL_FETCHING_RESULT =
            "Android.GridTabSwitcher.ThumbnailFetchingResult";

    private float mThumbnailScale;
    /**
     * The limit on the number of fullsized or ETC1 compressed thumbnails in the in-memory cache.
     * If in future there is a need for more bitmaps to be visible on the screen at once this value
     * can be increased.
     */
    private int mFullResThumbnailsMaxSize;
    private final ContentOffsetProvider mContentOffsetProvider;
    private long mNativeTabContentManager;

    private final ArrayList<ThumbnailChangeListener> mListeners =
            new ArrayList<ThumbnailChangeListener>();

    private boolean mSnapshotsEnabled;
    private final TabFinder mTabFinder;
    private final Context mContext;

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
     * The interface to get a {@link Tab} from a tab ID.
     */
    public interface TabFinder { Tab getTabById(int id); }

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
        if (TabUiFeatureUtilities.isGridTabSwitcherEnabled(context)) {
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
     * @param tabFinder             The helper function to get tab from an ID.
     */
    public TabContentManager(Context context, ContentOffsetProvider contentOffsetProvider,
            boolean snapshotsEnabled, TabFinder tabFinder) {
        mContext = context;
        mContentOffsetProvider = contentOffsetProvider;
        mTabFinder = tabFinder;
        mSnapshotsEnabled = snapshotsEnabled;

        // Override the cache size on the command line with --thumbnails=100
        int defaultCacheSize = getIntegerResourceWithOverride(
                mContext, R.integer.default_thumbnail_cache_size, ChromeSwitches.THUMBNAILS);

        mFullResThumbnailsMaxSize = defaultCacheSize;

        float thumbnailScale = 1.f;
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(mContext);
        float deviceDensity = display.getDipScale();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            // Scale all tablets to MDPI.
            thumbnailScale = 1.f / deviceDensity;
        } else {
            // For phones, reduce the amount of memory usage by capturing a lower-res thumbnail for
            // devices with resolution higher than HDPI (crbug.com/357740).
            if (deviceDensity > 1.5f) {
                thumbnailScale = 1.5f / deviceDensity;
            }
        }
        mThumbnailScale = thumbnailScale;
    }

    /**
     * Called after native library is loaded.
     */
    public void initWithNative() {
        int compressionQueueMaxSize =
                mContext.getResources().getInteger(R.integer.default_compression_queue_size);
        int writeQueueMaxSize =
                mContext.getResources().getInteger(R.integer.default_write_queue_size);

        // Override the cache size on the command line with
        // --approximation-thumbnails=100
        int approximationCacheSize = getIntegerResourceWithOverride(mContext,
                R.integer.default_approximation_thumbnail_cache_size,
                ChromeSwitches.APPROXIMATION_THUMBNAILS);

        boolean useApproximationThumbnails =
                !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                && !sThumbnailCacheRefactor.isEnabled();
        boolean saveJpegThumbnails = TabUiFeatureUtilities.isGridTabSwitcherEnabled(mContext);

        mNativeTabContentManager =
                TabContentManagerJni.get().init(TabContentManager.this, mFullResThumbnailsMaxSize,
                        approximationCacheSize, compressionQueueMaxSize, writeQueueMaxSize,
                        useApproximationThumbnails, saveJpegThumbnails, getTabCaptureAspectRatio());
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
    private Tab getTabById(int tabId) {
        if (mTabFinder == null) return null;

        return mTabFinder.getTabById(tabId);
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
        if (mNativeTabContentManager == 0 || sThumbnailCacheRefactor.isEnabled()) return;
        TabContentManagerJni.get().attachTab(mNativeTabContentManager, tab, tab.getId());
    }

    /**
     * Detach the given Tab's cc layer from this {@link TabContentManager}.
     * @param tab Tab whose cc layer will be detached.
     */
    public void detachTab(Tab tab) {
        if (mNativeTabContentManager == 0 || sThumbnailCacheRefactor.isEnabled()) return;
        TabContentManagerJni.get().detachTab(mNativeTabContentManager, tab, tab.getId());
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
            viewToDraw = tab.getView();
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

        int width = (int) ((viewToDraw.getMeasuredWidth() + leftMargin) * mThumbnailScale);
        int height = (int) ((viewToDraw.getMeasuredHeight() + topMargin - overlayTranslateY)
                * mThumbnailScale);
        if (width <= 0 || height <= 0) {
            return null;
        }

        try {
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
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
     * Call to get a thumbnail for a given tab through a {@link Callback}. If there is
     * no up-to-date thumbnail on disk for the given tab, callback returns null.
     * @param tabId The ID of the tab to get the thumbnail for.
     * @param thumbnailSize Desired size of thumbnail received by callback.
     * @param callback The callback to send the {@link Bitmap} with. Can be called up to twice when
     *                 {@code forceUpdate}; otherwise always called exactly once.
     * @param forceUpdate Whether to obtain the thumbnail from the live content.
     * @param writeBack When {@code forceUpdate}, whether to write the thumbnail to cache.
     */
    public void getTabThumbnailWithCallback(@NonNull int tabId, @NonNull Size thumbnailSize,
            @NonNull Callback<Bitmap> callback, boolean forceUpdate, boolean writeBack) {
        if (!mSnapshotsEnabled) return;

        if (!forceUpdate) {
            assert !writeBack : "writeBack is ignored if not forceUpdate";
            getTabThumbnailFromDisk(tabId, thumbnailSize, callback);
            return;
        }

        if (mNativeTabContentManager == 0) return;

        // Reading thumbnail from disk is faster than taking screenshot from live Tab, so fetch
        // that first even if |forceUpdate|.
        getTabThumbnailFromDisk(tabId, thumbnailSize, (diskBitmap) -> {
            if (diskBitmap != null) callback.onResult(diskBitmap);

            Tab tab = getTabById(tabId);
            if (tab == null) return;

            captureThumbnail(tab, writeBack, (bitmap) -> {
                // Null check to avoid having a Bitmap from getTabThumbnailFromDisk() but
                // cleared here.
                // If invalidation is not needed, readbackNativeBitmap() might not do anything
                // and send back null.
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
     * @param tabId The ID of the {@link Tab} the thumbnail is for.
     * @return The file storing the thumbnail in JPEG format of a certain {@link Tab}.
     */
    public static File getTabThumbnailFileJpeg(int tabId) {
        return new File(PathUtils.getThumbnailCacheDirectory(), tabId + ".jpeg");
    }

    @VisibleForTesting
    public static Bitmap getJpegForTab(int tabId, @NonNull Size thumbnailSize) {
        File file = getTabThumbnailFileJpeg(tabId);
        if (!file.isFile()) return null;
        if (thumbnailSize.getWidth() <= 0 || thumbnailSize.getHeight() <= 0) {
            return BitmapFactory.decodeFile(file.getPath());
        }
        return resizeJpeg(file.getPath(), thumbnailSize);
    }

    /**
     * See https://developer.android.com/topic/performance/graphics/load-bitmap#load-bitmap.
     * @param path Path of jpeg file.
     * @param thumbnailSize Desired thumbnail size to resize to.
     * @return Resized bitmap.
     */
    private static Bitmap resizeJpeg(String path, Size thumbnailSize) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(path, options);

        // Raw height and width of image
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > thumbnailSize.getHeight() || width > thumbnailSize.getWidth()) {
            final int halfHeight = height / 2;
            final int halfWidth = width / 2;

            // Calculate the largest inSampleSize value that is a power of 2 and keeps both
            // height and width larger than the requested height and width.
            while ((halfHeight / inSampleSize) >= thumbnailSize.getHeight()
                    && (halfWidth / inSampleSize) >= thumbnailSize.getWidth()) {
                inSampleSize *= 2;
            }
        }
        options.inSampleSize = inSampleSize;

        // Decode bitmap with inSampleSize set
        options.inJustDecodeBounds = false;
        return BitmapFactory.decodeFile(path, options);
    }

    private void getTabThumbnailFromDisk(
            @NonNull int tabId, @NonNull Size thumbnailSize, @NonNull Callback<Bitmap> callback) {
        // Try JPEG thumbnail first before using the more costly
        // TabContentManagerJni.get().getEtc1TabThumbnail.
        TraceEvent.startAsync("GetTabThumbnailFromDisk", tabId);
        PostTask.postTask(TaskTraits.USER_VISIBLE_MAY_BLOCK, () -> {
            Bitmap bitmap = getJpegForTab(tabId, thumbnailSize);
            PostTask.postTask(TaskTraits.UI_USER_VISIBLE,
                    () -> { onBitmapRead(tabId, thumbnailSize, bitmap, callback); });
        });
    }

    private void onBitmapRead(@NonNull int tabId, @NonNull Size thumbnailSize, Bitmap jpeg,
            @NonNull Callback<Bitmap> callback) {
        TraceEvent.finishAsync("GetTabThumbnailFromDisk", tabId);
        if (jpeg != null) {
            // Pixel difference between the real aspect ratio and the actual aspect ratio.
            final int aspectRatioPixelError =
                    Math.abs((int) Math.round(jpeg.getHeight() * getTabCaptureAspectRatio())
                            - jpeg.getWidth());
            // Allow a pixel error proportional to the size of the thumbnail that will be shown.
            // thumbnailSize will be within a factor of 2 of the size of jpeg due to resizeJpeg.
            final int aspectRatioAllowedError =
                    (int) Math.round(Math.min(thumbnailSize.getWidth(), thumbnailSize.getHeight())
                            * PIXEL_TOLERANCE_PERCENT);
            if (aspectRatioPixelError >= aspectRatioAllowedError) {
                recordThumbnailFetchingResult(
                        ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

                if (mNativeTabContentManager == 0) {
                    callback.onResult(jpeg);
                    return;
                }
                if (!mSnapshotsEnabled) return;

                TabContentManagerJni.get().getEtc1TabThumbnail(
                        mNativeTabContentManager, tabId, getTabCaptureAspectRatio(), callback);
                return;
            }
            recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_JPEG);

            callback.onResult(jpeg);
            return;
        }
        if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;
        TabContentManagerJni.get().getEtc1TabThumbnail(
                mNativeTabContentManager, tabId, getTabCaptureAspectRatio(), (etc1) -> {
                    if (etc1 != null) {
                        recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_ETC1);
                    } else {
                        recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_NOTHING);
                    }
                    callback.onResult(etc1);
                });
    }

    private static void recordThumbnailFetchingResult(@ThumbnailFetchingResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_THUMBNAIL_FETCHING_RESULT, result, ThumbnailFetchingResult.NUM_ENTRIES);
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
        TabContentManagerJni.get().cacheTabWithBitmap(mNativeTabContentManager, tab, nativeBitmap,
                mThumbnailScale, getTabCaptureAspectRatio());
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
            // TabContentManagerJni.get().cacheTabWithBitmap(), the logic
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
                    TabUiFeatureUtilities.isTabThumbnailAspectRatioNotOne()
                            ? Math.min(bitmap.getHeight(),
                                    (int) (bitmap.getWidth() * 1.0 / getTabCaptureAspectRatio()))
                            : min(bitmap.getWidth(), bitmap.getHeight()),
                    matrix, true);
            callback.onResult(resized);
        } else {
            if (tab.getWebContents() == null) return;
            // If we don't have to write the thumbnail back to the cache, we can use the faster
            // path of capturing a downsampled copy.
            // This faster path is essential to Tab-to-Grid animation to be smooth.
            final float downsamplingScale = writeToCache ? 1 : 0.5f;
            TabContentManagerJni.get().captureThumbnail(mNativeTabContentManager, tab,
                    mThumbnailScale * downsamplingScale, writeToCache, getTabCaptureAspectRatio(),
                    callback);
        }
    }

    private double getTabCaptureAspectRatio() {
        return TabUtils.getTabThumbnailAspectRatio(mContext);
    }

    /**
     * Invalidate a thumbnail if the content of the tab has been changed.
     * @param tabId The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateIfChanged(int tabId, GURL url) {
        if (mNativeTabContentManager != 0) {
            TabContentManagerJni.get().invalidateIfChanged(mNativeTabContentManager, tabId, url);
        }
    }

    /**
     * Update the priority-ordered list of visible tabs.
     * @param priority The list of tab ids to load cached thumbnails for. Only the first
     *                 {@link mFullResThumbnailsMaxSize} thumbnails will be loaded.
     * @param primaryTabId The id of the current tab this is not loaded under the assumption it will
     *                     have a live layer. If this is not the case it should be the first tab in
     *                     the priority list.
     */
    public void updateVisibleIds(List<Integer> priority, int primaryTabId) {
        if (mNativeTabContentManager != 0) {
            int idsSize = min(mFullResThumbnailsMaxSize, priority.size());
            int[] priorityIds = new int[idsSize];
            for (int i = 0; i < idsSize; i++) {
                priorityIds[i] = priority.get(i);
            }
            TabContentManagerJni.get().updateVisibleIds(
                    mNativeTabContentManager, priorityIds, primaryTabId);
        }
    }

    /**
     * Removes a thumbnail of the tab whose id is |tabId|.
     * @param tabId The Id of the tab whose thumbnail is being removed.
     */
    public void removeTabThumbnail(int tabId) {
        if (mNativeTabContentManager != 0) {
            TabContentManagerJni.get().removeTabThumbnail(mNativeTabContentManager, tabId);
        }
    }

    @VisibleForTesting
    public void setCaptureMinRequestTimeForTesting(int timeMs) {
        TabContentManagerJni.get().setCaptureMinRequestTimeForTesting(
                mNativeTabContentManager, timeMs);
    }

    @VisibleForTesting
    public int getPendingReadbacksForTesting() {
        return TabContentManagerJni.get().getPendingReadbacksForTesting(mNativeTabContentManager);
    }

    @CalledByNative
    protected void notifyListenersOfThumbnailChange(int tabId) {
        for (ThumbnailChangeListener listener : mListeners) {
            listener.onThumbnailChange(tabId);
        }
    }

    private boolean isNativeViewShowing(Tab tab) {
        return tab != null && tab.isShowingCustomView();
    }

    @NativeMethods
    interface Natives {
        // Class Object Methods
        long init(TabContentManager caller, int defaultCacheSize, int approximationCacheSize,
                int compressionQueueMaxSize, int writeQueueMaxSize,
                boolean useApproximationThumbnail, boolean saveJpegThumbnails,
                double jpegAspectRatio);

        void attachTab(long nativeTabContentManager, Tab tab, int tabId);
        void detachTab(long nativeTabContentManager, Tab tab, int tabId);
        void captureThumbnail(long nativeTabContentManager, Object tab, float thumbnailScale,
                boolean writeToCache, double aspectRatio, Callback<Bitmap> callback);
        void cacheTabWithBitmap(long nativeTabContentManager, Object tab, Object bitmap,
                float thumbnailScale, double aspectRatio);
        void invalidateIfChanged(long nativeTabContentManager, int tabId, GURL url);
        void updateVisibleIds(long nativeTabContentManager, int[] priority, int primaryTabId);
        void removeTabThumbnail(long nativeTabContentManager, int tabId);
        void getEtc1TabThumbnail(long nativeTabContentManager, int tabId, double aspectRatio,
                Callback<Bitmap> callback);
        void setCaptureMinRequestTimeForTesting(long nativeTabContentManager, int timeMs);
        int getPendingReadbacksForTesting(long nativeTabContentManager);
        void destroy(long nativeTabContentManager);
    }
}
