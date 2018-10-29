// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.LruCache;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.RelativeLayout;

import org.chromium.base.DiscardableReferencePool.DiscardableReference;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A class for keeping track of common data associated with showing photos in
 * the photo picker, for example the RecyclerView and the bitmap caches.
 */
public class PickerCategoryView extends RelativeLayout
        implements FileEnumWorkerTask.FilesEnumeratedCallback, RecyclerView.RecyclerListener,
                   DecoderServiceHost.ServiceReadyCallback, View.OnClickListener {
    // These values are written to logs.  New enum values can be added, but existing
    // enums must never be renumbered or deleted and reused.
    private static final int ACTION_CANCEL = 0;
    private static final int ACTION_PHOTO_PICKED = 1;
    private static final int ACTION_NEW_PHOTO = 2;
    private static final int ACTION_BROWSE = 3;
    private static final int ACTION_BOUNDARY = 4;

    // The dialog that owns us.
    private PhotoPickerDialog mDialog;

    // The view containing the RecyclerView and the toolbar, etc.
    private SelectableListLayout<PickerBitmap> mSelectableListLayout;

    // Our activity.
    private ChromeActivity mActivity;

    // The list of images on disk, sorted by last-modified first.
    private List<PickerBitmap> mPickerBitmaps;

    // True if multi-selection is allowed in the picker.
    private boolean mMultiSelectionAllowed;

    // The callback to notify the listener of decisions reached in the picker.
    private PhotoPickerListener mListener;

    // The host class for the decoding service.
    private DecoderServiceHost mDecoderServiceHost;

    // The RecyclerView showing the images.
    private RecyclerView mRecyclerView;

    // The {@link PickerAdapter} for the RecyclerView.
    private PickerAdapter mPickerAdapter;

    // The layout manager for the RecyclerView.
    private GridLayoutManager mLayoutManager;

    // The decoration to use for the RecyclerView.
    private GridSpacingItemDecoration mSpacingDecoration;

    // The {@link SelectionDelegate} keeping track of which images are selected.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // A low-resolution cache for images, lazily created. Helpful for cache misses from the
    // high-resolution cache to avoid showing gray squares (we show pixelated versions instead until
    // image can be loaded off disk, which is much less jarring).
    private DiscardableReference<LruCache<String, Bitmap>> mLowResBitmaps;

    // A high-resolution cache for images, lazily created.
    private DiscardableReference<LruCache<String, Bitmap>> mHighResBitmaps;

    // The size of the low-res cache.
    private int mCacheSizeLarge;

    // The size of the high-res cache.
    private int mCacheSizeSmall;

    /**
     * The number of columns to show. Note: mColumns and mPadding (see below) should both be even
     * numbers or both odd, not a mix (the column padding will not be of uniform thickness if they
     * are a mix).
     */
    private int mColumns;

    // The padding between columns. See also comment for mColumns.
    private int mPadding;

    // The size of the bitmaps (equal length for width and height).
    private int mImageSize;

    // A worker task for asynchronously enumerating files off the main thread.
    private FileEnumWorkerTask mWorkerTask;

    // The timestap for the start of the enumeration of files on disk.
    private long mEnumStartTime;

    // Whether the connection to the service has been established.
    private boolean mServiceReady;

    // The MIME types requested.
    private List<String> mMimeTypes;

    // A list of files to use for testing (instead of reading files on disk).
    private static List<PickerBitmap> sTestFiles;

    /**
     * @param context The context to use.
     * @param multiSelectionAllowed Whether to allow the user to select more than one image.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public PickerCategoryView(Context context, boolean multiSelectionAllowed) {
        super(context);
        mActivity = (ChromeActivity) context;
        mMultiSelectionAllowed = multiSelectionAllowed;

        mDecoderServiceHost = new DecoderServiceHost(this, context);
        mDecoderServiceHost.bind(context);

        mSelectionDelegate = new SelectionDelegate<PickerBitmap>();
        if (!multiSelectionAllowed) mSelectionDelegate.setSingleSelectionMode();

        View root = LayoutInflater.from(context).inflate(R.layout.photo_picker_dialog, this);
        mSelectableListLayout =
                (SelectableListLayout<PickerBitmap>) root.findViewById(R.id.selectable_list);

        mPickerAdapter = new PickerAdapter(this);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mPickerAdapter);
        int titleId = multiSelectionAllowed ? R.string.photo_picker_select_images
                                            : R.string.photo_picker_select_image;
        PhotoPickerToolbar toolbar = (PhotoPickerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.photo_picker_toolbar, mSelectionDelegate, titleId, null, 0, 0,
                R.color.default_primary_color, null, false, false);
        toolbar.setNavigationOnClickListener(this);
        Button doneButton = (Button) toolbar.findViewById(R.id.done);
        doneButton.setOnClickListener(this);

        calculateGridMetrics();

        mLayoutManager = new GridLayoutManager(mActivity, mColumns);
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mSpacingDecoration = new GridSpacingItemDecoration(mColumns, mPadding);
        mRecyclerView.addItemDecoration(mSpacingDecoration);
        mRecyclerView.setRecyclerListener(this);

        final long maxMemory = ConversionUtils.bytesToKilobytes(Runtime.getRuntime().maxMemory());
        mCacheSizeLarge = (int) (maxMemory / 2); // 1/2 of the available memory.
        mCacheSizeSmall = (int) (maxMemory / 8); // 1/8th of the available memory.
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        calculateGridMetrics();
        mLayoutManager.setSpanCount(mColumns);
        mRecyclerView.removeItemDecoration(mSpacingDecoration);
        mSpacingDecoration = new GridSpacingItemDecoration(mColumns, mPadding);
        mRecyclerView.addItemDecoration(mSpacingDecoration);
    }

    /**
     * Severs the connection to the decoding utility process and cancels any outstanding requests.
     */
    public void onDialogDismissed() {
        if (mWorkerTask != null) {
            mWorkerTask.cancel(true);
            mWorkerTask = null;
        }

        if (mDecoderServiceHost != null) {
            mDecoderServiceHost.unbind(mActivity);
            mDecoderServiceHost = null;
        }
    }

    /**
     * Initializes the PickerCategoryView object.
     * @param dialog The dialog showing us.
     * @param listener The listener who should be notified of actions.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public void initialize(
            PhotoPickerDialog dialog, PhotoPickerListener listener, List<String> mimeTypes) {
        mDialog = dialog;
        mListener = listener;
        mMimeTypes = new ArrayList<>(mimeTypes);

        enumerateBitmaps();

        mDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                executeAction(PhotoPickerListener.PhotoPickerAction.CANCEL, null, ACTION_CANCEL);
            }
        });
    }

    // FileEnumWorkerTask.FilesEnumeratedCallback:

    @Override
    public void filesEnumeratedCallback(List<PickerBitmap> files) {
        // Calculate the rate of files enumerated per tenth of a second.
        long elapsedTimeMs = SystemClock.elapsedRealtime() - mEnumStartTime;
        int rate = (int) (100 * files.size() / elapsedTimeMs);
        RecordHistogram.recordTimesHistogram(
                "Android.PhotoPicker.EnumerationTime", elapsedTimeMs, TimeUnit.MILLISECONDS);
        RecordHistogram.recordCustomCountHistogram(
                "Android.PhotoPicker.EnumeratedFiles", files.size(), 1, 10000, 50);
        RecordHistogram.recordCount1000Histogram("Android.PhotoPicker.EnumeratedRate", rate);

        mPickerBitmaps = files;
        processBitmaps();
    }

    // DecoderServiceHost.ServiceReadyCallback:

    @Override
    public void serviceReady() {
        mServiceReady = true;
        processBitmaps();
    }

    // RecyclerView.RecyclerListener:

    @Override
    public void onViewRecycled(RecyclerView.ViewHolder holder) {
        PickerBitmapViewHolder bitmapHolder = (PickerBitmapViewHolder) holder;
        String filePath = bitmapHolder.getFilePath();
        if (filePath != null) {
            getDecoderServiceHost().cancelDecodeImage(filePath);
        }
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.done) {
            notifyPhotosSelected();
        } else {
            executeAction(PhotoPickerListener.PhotoPickerAction.CANCEL, null, ACTION_CANCEL);
        }
    }

    /**
     * Start loading of bitmaps, once files have been enumerated and service is
     * ready to decode.
     */
    private void processBitmaps() {
        if (mServiceReady && mPickerBitmaps != null) {
            mPickerAdapter.notifyDataSetChanged();
        }
    }

    // Simple accessors:

    public int getImageSize() {
        return mImageSize;
    }

    public SelectionDelegate<PickerBitmap> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    public List<PickerBitmap> getPickerBitmaps() {
        return mPickerBitmaps;
    }

    public DecoderServiceHost getDecoderServiceHost() {
        return mDecoderServiceHost;
    }

    public LruCache<String, Bitmap> getLowResBitmaps() {
        if (mLowResBitmaps == null || mLowResBitmaps.get() == null) {
            mLowResBitmaps =
                    mActivity.getReferencePool().put(new LruCache<String, Bitmap>(mCacheSizeSmall));
        }
        return mLowResBitmaps.get();
    }

    public LruCache<String, Bitmap> getHighResBitmaps() {
        if (mHighResBitmaps == null || mHighResBitmaps.get() == null) {
            mHighResBitmaps =
                    mActivity.getReferencePool().put(new LruCache<String, Bitmap>(mCacheSizeLarge));
        }
        return mHighResBitmaps.get();
    }

    public boolean isMultiSelectAllowed() {
        return mMultiSelectionAllowed;
    }

    /**
     * Notifies the listener that the user selected to launch the gallery.
     */
    public void showGallery() {
        executeAction(PhotoPickerListener.PhotoPickerAction.LAUNCH_GALLERY, null, ACTION_BROWSE);
    }

    /**
     * Notifies the listener that the user selected to launch the camera intent.
     */
    public void showCamera() {
        executeAction(PhotoPickerListener.PhotoPickerAction.LAUNCH_CAMERA, null, ACTION_NEW_PHOTO);
    }

    /**
     * Calculates image size and how many columns can fit on-screen.
     */
    private void calculateGridMetrics() {
        Rect appRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(appRect);

        int width = appRect.width();
        int minSize =
                mActivity.getResources().getDimensionPixelSize(R.dimen.photo_picker_tile_min_size);
        mPadding = mActivity.getResources().getDimensionPixelSize(R.dimen.photo_picker_tile_gap);
        mColumns = Math.max(1, (width - mPadding) / (minSize + mPadding));
        mImageSize = (width - mPadding * (mColumns + 1)) / (mColumns);

        // Make sure columns and padding are either both even or both odd.
        if (((mColumns % 2) == 0) != ((mPadding % 2) == 0)) {
            mPadding++;
        }
    }

    /**
     * Asynchronously enumerates bitmaps on disk.
     */
    private void enumerateBitmaps() {
        if (sTestFiles != null) {
            filesEnumeratedCallback(sTestFiles);
            return;
        }

        if (mWorkerTask != null) {
            mWorkerTask.cancel(true);
        }

        mEnumStartTime = SystemClock.elapsedRealtime();
        mWorkerTask = new FileEnumWorkerTask(
                mActivity.getWindowAndroid(), this, new MimeTypeFileFilter(mMimeTypes));
        mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Notifies any listeners that one or more photos have been selected.
     */
    private void notifyPhotosSelected() {
        List<PickerBitmap> selectedFiles = mSelectionDelegate.getSelectedItemsAsList();
        Collections.sort(selectedFiles);
        String[] photos = new String[selectedFiles.size()];
        int i = 0;
        for (PickerBitmap bitmap : selectedFiles) {
            photos[i++] = bitmap.getFilePath();
        }

        executeAction(
                PhotoPickerListener.PhotoPickerAction.PHOTOS_SELECTED, photos, ACTION_PHOTO_PICKED);
    }

    /**
     * A class for implementing grid spacing between items.
     */
    private class GridSpacingItemDecoration extends RecyclerView.ItemDecoration {
        // The number of spans to account for.
        private int mSpanCount;

        // The amount of spacing to use.
        private int mSpacing;

        public GridSpacingItemDecoration(int spanCount, int spacing) {
            mSpanCount = spanCount;
            mSpacing = spacing;
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            int left = 0, right = 0, top = 0, bottom = 0;
            int position = parent.getChildAdapterPosition(view);

            if (position >= 0) {
                int column = position % mSpanCount;

                left = mSpacing - ((column * mSpacing) / mSpanCount);
                right = (column + 1) * mSpacing / mSpanCount;

                if (position < mSpanCount) {
                    top = mSpacing;
                }
                bottom = mSpacing;
            }

            outRect.set(left, top, right, bottom);
        }
    }

    /**
     * Report back what the user selected in the dialog, report UMA and clean up.
     * @param action The action taken.
     * @param photos The photos that were selected (if any).
     * @param umaId The UMA value to record with the action.
     */
    private void executeAction(
            @PhotoPickerListener.PhotoPickerAction int action, String[] photos, int umaId) {
        mListener.onPhotoPickerUserAction(action, photos);
        mDialog.dismiss();
        UiUtils.onPhotoPickerDismissed();
        recordFinalUmaStats(umaId);
    }

    /**
     * Record UMA statistics (what action was taken in the dialog and other performance stats).
     * @param action The action the user took in the dialog.
     */
    private void recordFinalUmaStats(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PhotoPicker.DialogAction", action, ACTION_BOUNDARY);
        RecordHistogram.recordCountHistogram(
                "Android.PhotoPicker.DecodeRequests", mPickerAdapter.getDecodeRequestCount());
        RecordHistogram.recordCountHistogram(
                "Android.PhotoPicker.CacheHits", mPickerAdapter.getCacheHitCount());
    }

    /** Sets a list of files to use as data for the dialog. For testing use only. */
    @VisibleForTesting
    public static void setTestFiles(List<PickerBitmap> testFiles) {
        sTestFiles = new ArrayList<>(testFiles);
    }

    @VisibleForTesting
    public SelectionDelegate<PickerBitmap> getSelectionDelegateForTesting() {
        return mSelectionDelegate;
    }
}
