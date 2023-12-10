// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Owns the lifecycle of one bitmap in the long screenshot.
 * <ul>
 * <li>1. Defines the bounds for the bitmap.</li>
 * <li>2. Requests the generation of the bitmap using {@BitmapGenerator}.</li>
 * <li>3. Tracks the status of the generation.</li>
 * <li>4. Stores the generated bitmap.</li>
 * </ul>
 * Callers of this class should provide a {@link LongScreenshotsEntry.EntryListener} which returns
 * the status of the generation. Upon receiving the BITMAP_GENERATED success code, callers can call
 * {@link getBitmap} to retrieve the generated bitmap.
 */
public class LongScreenshotsEntry {
    private Rect mRect;
    private BitmapGenerator mGenerator;
    private @EntryStatus int mCurrentStatus;

    // Generated bitmap
    private Bitmap mGeneratedBitmap;
    private EntryListener mEntryListener;
    private Callback<Integer> mMemoryTracker;

    @IntDef({
        EntryStatus.UNKNOWN,
        EntryStatus.INSUFFICIENT_MEMORY,
        EntryStatus.GENERATION_ERROR,
        EntryStatus.BITMAP_GENERATED,
        EntryStatus.CAPTURE_COMPLETE,
        EntryStatus.CAPTURE_IN_PROGRESS,
        EntryStatus.BITMAP_GENERATION_IN_PROGRESS,
        EntryStatus.BOUNDS_ABOVE_CAPTURE,
        EntryStatus.BOUNDS_BELOW_CAPTURE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryStatus {
        int UNKNOWN = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int BITMAP_GENERATED = 3;
        int CAPTURE_COMPLETE = 4;
        int CAPTURE_IN_PROGRESS = 5;
        int BITMAP_GENERATION_IN_PROGRESS = 6;
        int BOUNDS_ABOVE_CAPTURE = 7;
        int BOUNDS_BELOW_CAPTURE = 8;
    }

    /**
     * Users of the {@link LongScreenshotsEntry} class should implement and pass the listener to the
     * entry to be aware of bitmap generation status updates.
     */
    public interface EntryListener {
        /**
         * Called with a status update. This could be a success or a failure. On success, callers
         * should call {@link getBitmap} to get the bitmap.
         */
        void onResult(@EntryStatus int status);
    }

    /**
     * @param generator BitmapGenerator to be used to capture and composite the website.
     * @param bounds The bounds of the entry.
     * @param memoryTracker Callback to be notified of the entry's memory usage.
     */
    public LongScreenshotsEntry(
            BitmapGenerator generator, Rect bounds, Callback<Integer> memoryTracker) {
        mRect = bounds;
        mGenerator = generator;
        mMemoryTracker = memoryTracker;
    }

    static LongScreenshotsEntry createEntryWithStatus(@EntryStatus int status) {
        LongScreenshotsEntry entry = new LongScreenshotsEntry(null, null, null);
        entry.updateStatus(status);
        return entry;
    }

    /**
     * @param listener listens for the status update.
     */
    public void setListener(EntryListener listener) {
        mEntryListener = listener;

        if (mCurrentStatus != EntryStatus.UNKNOWN) {
            updateStatus(mCurrentStatus);
        }
    }

    /**
     * The start Y axis acts as the id of the entry as it will be unique.
     *
     * @return the id of this entry.
     */
    public int getId() {
        return mRect == null ? -1 : mRect.top;
    }

    int getEndYAxis() {
        return mRect == null ? -1 : mRect.bottom;
    }

    void generateBitmap() {
        if (mGenerator == null) {
            updateStatus(EntryStatus.GENERATION_ERROR);
            return;
        }
        updateStatus(EntryStatus.BITMAP_GENERATION_IN_PROGRESS);
        mGenerator.compositeBitmap(mRect, this::onBitmapGenerationError, this::onBitmapGenerated);
    }

    /**
     * @return The current status of the generation.
     */
    public @EntryStatus int getStatus() {
        return mCurrentStatus;
    }

    /**
     * @return the generated bitmap or null in the case of error or incomplete generation. Callers
     *         should only call this function after listening their EntryListener gets called with a
     *         status update.
     */
    public Bitmap getBitmap() {
        return mGeneratedBitmap;
    }

    private void onBitmapGenerated(Bitmap bitmap) {
        mGeneratedBitmap = bitmap;

        if (mMemoryTracker != null && mGeneratedBitmap != null) {
            mMemoryTracker.onResult(mGeneratedBitmap.getAllocationByteCount());
        }
        updateStatus(EntryStatus.BITMAP_GENERATED);
    }

    private void onBitmapGenerationError() {
        updateStatus(EntryStatus.GENERATION_ERROR);
    }

    void updateStatus(@EntryStatus int status) {
        mCurrentStatus = status;
        if (mEntryListener != null) {
            mEntryListener.onResult(mCurrentStatus);
        }
    }

    void destroy() {
        if (mGenerator != null) {
            mGenerator.destroy();
            mGenerator = null;
        }
        mGeneratedBitmap = null;
    }
}
