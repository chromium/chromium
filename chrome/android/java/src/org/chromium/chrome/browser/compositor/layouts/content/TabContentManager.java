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
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
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
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The TabContentManager is responsible for serving tab contents to the UI components. Contents
 * could be live or static thumbnails.
 */
@JNINamespace("android")
public class TabContentManager {
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
    public static final double ASPECT_RATIO_PRECISION = 0.01;

    // Whether to allow to refetch tab thumbnail if the aspect ratio is not matching.
    public static final BooleanCachedFieldTrialParameter ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, "allow_to_refetch", true);

    @VisibleForTesting
    public static final String UMA_THUMBNAIL_FETCHING_RESULT =
            "GridTabSwitcher.ThumbnailFetchingResult";
    private final Set<Integer> mRefectchedTabIds = new HashSet<>();

    private float mThumbnailScale;
    private int mFullResThumbnailsMaxSize;
    private final ContentOffsetProvider mContentOffsetProvider;
    private int[] mPriorityTabIds;
    private long mNativeTabContentManager;

    private final ArrayList<ThumbnailChangeListener> mListeners =
            new ArrayList<ThumbnailChangeListener>();

    private boolean mSnapshotsEnabled;
    private final TabFinder mTabFinder;
    private final Context mContext;

    /**
     * Listener to receive the "Last Thumbnail" event. "Last Thumbnail" is the first time
     * in the Activity life cycle that all the thumbnails in the Grid Tab Switcher are shown.
     */
    public interface LastThumbnailListener { void onLastThumbnail(int numOfThumbnails); }
    private boolean mLastThumbnailHappened;
    private List<LastThumbnailListener> mLastThumbnailListeners;
    private int mOnTheFlyRequests;
    private int mRequests;
    private int mNumOfThumbnailsForLastThumbnail;
    private float mExpectedThumbnailAspectRatio;

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

        mPriorityTabIds = new int[mFullResThumbnailsMaxSize];
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
                !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
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
        if (mRefectchedTabIds != null) mRefectchedTabIds.clear();
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
     * @param tabId The ID of the tab to get the thumbnail for.
     * @param thumbnailSize Desired size of thumbnail received by callback. Use default values if
     *         null.
     * @param callback The callback to send the {@link Bitmap} with. Can be called up to twice when
     *                 {@code forceUpdate}; otherwise always called exactly once.
     * @param forceUpdate Whether to obtain the thumbnail from the live content.
     * @param writeBack When {@code forceUpdate}, whether to write the thumbnail to cache.
     */
    public void getTabThumbnailWithCallback(@NonNull int tabId, @Nullable Size thumbnailSize,
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

            if (mTabFinder == null) return;

            Tab tab = mTabFinder.getTabById(tabId);
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

    /**
     * Add a listener to receive the "Last Thumbnail" event.
     * Note that this should not be called when there are no tabs.
     * @param listener A {@link LastThumbnailListener} to be called at the event. Must post the
     *                 real task and finish immediately.
     */
    public void addOnLastThumbnailListener(LastThumbnailListener listener) {
        ThreadUtils.assertOnUiThread();

        if (mLastThumbnailListeners == null) mLastThumbnailListeners = new ArrayList<>();
        mLastThumbnailListeners.add(listener);
        if (mLastThumbnailHappened) notifyOnLastThumbnail();
    }

    private void notifyOnLastThumbnail() {
        ThreadUtils.assertOnUiThread();

        if (mLastThumbnailListeners != null) {
            for (LastThumbnailListener c : mLastThumbnailListeners) {
                c.onLastThumbnail(mNumOfThumbnailsForLastThumbnail);
            }
            mLastThumbnailListeners = null;
        }
    }

    @VisibleForTesting
    public static Bitmap getJpegForTab(int tabId, @Nullable Size thumbnailSize) {
        File file = getTabThumbnailFileJpeg(tabId);
        if (!file.isFile()) return null;
        if (thumbnailSize == null || thumbnailSize.getWidth() <= 0
                || thumbnailSize.getHeight() <= 0) {
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
            @NonNull int tabId, @Nullable Size thumbnailSize, @NonNull Callback<Bitmap> callback) {
        mOnTheFlyRequests++;
        mRequests++;
        // Try JPEG thumbnail first before using the more costly
        // TabContentManagerJni.get().getEtc1TabThumbnail.
        TraceEvent.startAsync("GetTabThumbnailFromDisk", tabId);
        new AsyncTask<Bitmap>() {
            @Override
            public Bitmap doInBackground() {
                return getJpegForTab(tabId, thumbnailSize);
            }

            @Override
            public void onPostExecute(Bitmap jpeg) {
                TraceEvent.finishAsync("GetTabThumbnailFromDisk", tabId);
                mOnTheFlyRequests--;
                if (mOnTheFlyRequests == 0 && !mLastThumbnailHappened) {
                    mLastThumbnailHappened = true;
                    mNumOfThumbnailsForLastThumbnail = mRequests;
                    notifyOnLastThumbnail();
                }
                if (jpeg != null) {
                    if (ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION.getValue()) {
                        // TODO(crbug.com/1344354): compare the height instead of pixel tolerance.
                        double jpegAspectRatio = jpeg.getHeight() == 0
                                ? 0
                                : 1.0 * jpeg.getWidth() / jpeg.getHeight();
                        // Retry fetching thumbnail once for all tabs that are:
                        //  * Thumbnail's aspect ratio is different from the expected ratio.
                        if (!mRefectchedTabIds.contains(tabId)
                                && Math.abs(jpegAspectRatio - getTabCaptureAspectRatio())
                                        >= ASPECT_RATIO_PRECISION) {
                            recordThumbnailFetchingResult(
                                    ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

                            if (mNativeTabContentManager == 0) {
                                callback.onResult(jpeg);
                                return;
                            }
                            if (!mSnapshotsEnabled) return;

                            mRefectchedTabIds.add(tabId);
                            TabContentManagerJni.get().getEtc1TabThumbnail(mNativeTabContentManager,
                                    TabContentManager.this, tabId, getTabCaptureAspectRatio(),
                                    callback);
                            return;
                        }
                    }
                    recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_JPEG);

                    callback.onResult(jpeg);
                    return;
                }
                if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;
                TabContentManagerJni.get().getEtc1TabThumbnail(mNativeTabContentManager,
                        TabContentManager.this, tabId, getTabCaptureAspectRatio(), (etc1) -> {
                            if (etc1 != null) {
                                recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_ETC1);
                            } else {
                                recordThumbnailFetchingResult(ThumbnailFetchingResult.GOT_NOTHING);
                            }
                            callback.onResult(etc1);
                        });
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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
        TabContentManagerJni.get().cacheTabWithBitmap(mNativeTabContentManager,
                TabContentManager.this, tab, nativeBitmap, mThumbnailScale,
                getTabCaptureAspectRatio());
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
            TabContentManagerJni.get().captureThumbnail(mNativeTabContentManager,
                    TabContentManager.this, tab, mThumbnailScale * downsamplingScale, writeToCache,
                    getTabCaptureAspectRatio(), callback);
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
            TabContentManagerJni.get().invalidateIfChanged(
                    mNativeTabContentManager, TabContentManager.this, tabId, url);
        }
    }

    /**
     * Invalidate a thumbnail of the tab whose id is |id|.
     * @param id The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateTabThumbnail(int id, GURL url) {
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
        return tab != null && tab.isShowingCustomView();
    }

    @NativeMethods
    interface Natives {
        // Class Object Methods
        long init(TabContentManager caller, int defaultCacheSize, int approximationCacheSize,
                int compressionQueueMaxSize, int writeQueueMaxSize,
                boolean useApproximationThumbnail, boolean saveJpegThumbnails,
                double jpegAspectRatio);

        void attachTab(long nativeTabContentManager, TabContentManager caller, Tab tab, int tabId);
        void detachTab(long nativeTabContentManager, TabContentManager caller, Tab tab, int tabId);
        boolean hasFullCachedThumbnail(
                long nativeTabContentManager, TabContentManager caller, int tabId);
        void captureThumbnail(long nativeTabContentManager, TabContentManager caller, Object tab,
                float thumbnailScale, boolean writeToCache, double aspectRatio,
                Callback<Bitmap> callback);
        void cacheTabWithBitmap(long nativeTabContentManager, TabContentManager caller, Object tab,
                Object bitmap, float thumbnailScale, double aspectRatio);
        void invalidateIfChanged(
                long nativeTabContentManager, TabContentManager caller, int tabId, GURL url);
        void updateVisibleIds(long nativeTabContentManager, TabContentManager caller,
                int[] priority, int primaryTabId);
        void removeTabThumbnail(long nativeTabContentManager, TabContentManager caller, int tabId);
        void getEtc1TabThumbnail(long nativeTabContentManager, TabContentManager caller, int tabId,
                double aspectRatio, Callback<Bitmap> callback);
        void setCaptureMinRequestTimeForTesting(
                long nativeTabContentManager, TabContentManager caller, int timeMs);
        int getPendingReadbacksForTesting(long nativeTabContentManager, TabContentManager caller);
        void destroy(long nativeTabContentManager);
    }
}
