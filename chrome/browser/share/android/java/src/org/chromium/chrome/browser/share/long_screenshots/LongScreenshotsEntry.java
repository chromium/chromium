// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.content_public.browser.RenderCoordinates;

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
 *
 * Callers of this class should provide a {@link LongScreenshotsEntry.EntryListener} which returns
 * the status of the generation. Upon receiving the BITMAP_GENERATED success code, callers can call
 * {@link getBitmap} to retrieve the generated bitmap.
 */
public class LongScreenshotsEntry {
    private Context mContext;
    private Rect mRect;
    private Tab mTab;
    private BitmapGenerator mGenerator;
    private @EntryStatus int mCurrentStatus;

    // Generated bitmap
    private Bitmap mGeneratedBitmap;
    private EntryListener mEntryListener;

    @IntDef({EntryStatus.UNKNOWN, EntryStatus.INSUFFICIENT_MEMORY, EntryStatus.GENERATION_ERROR,
            EntryStatus.BITMAP_GENERATED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryStatus {
        int UNKNOWN = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int BITMAP_GENERATED = 3;
    }

    /**
     * Users of the {@link LongScreenshotsEntry} class should implement and pass the listener
     * to the entry to be aware of bitmap generation status updates.
     */
    public interface EntryListener {
        /**
         * Called with a status update. This could be a success or a failure. On success, callers
         * should call {@link getBitmap} to get the bitmap.
         */
        void onResult(@EntryStatus int status);
    }

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab The tab to capture the results for.
     * @param startYAxis Top coordinate to start capture from.
     * @param clipHeight Height of the capture.
     */
    public LongScreenshotsEntry(Context context, Tab tab, int startYAxis, int clipHeight) {
        mContext = context;
        mTab = tab;
        calculateClipBounds(startYAxis, clipHeight);
    }

    public void generateBitmap() {
        if (mGenerator == null) {
            mGenerator =
                    new BitmapGenerator(mContext, mTab, mRect, createBitmapGeneratorCallback());
        }
        mGenerator.captureScreenshot();
    }

    public void setListener(EntryListener listener) {
        mEntryListener = listener;
    }

    /**
     * The start Y axis acts as the id of the entry as it will be unique.
     *
     * @return the id of this entry.
     */
    public int getId() {
        return mRect.top;
    }

    private void updateStatus(@EntryStatus int status) {
        mCurrentStatus = status;
        if (mEntryListener != null) {
            mEntryListener.onResult(mCurrentStatus);
        }
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

    /**
     * Defines the bounds of the capture and compositing. Only the starting height and the height of
     * the clip is needed. The entire width is always captured.
     *
     * @param startYAxis Where on the scrolled page the capture and compositing should start.
     * @param clipHeight The length of the webpage that should be captured.
     */
    private void calculateClipBounds(int startYAxis, int clipHeight) {
        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());

        int endYAxis = (int) Math.floor(startYAxis + (clipHeight * coords.getPageScaleFactor()));
        int endXAxis =
                (int) Math.floor(coords.getContentWidthPixInt() / coords.getPageScaleFactor());
        mRect = new Rect(0, startYAxis, endXAxis, endYAxis);
    }

    @VisibleForTesting
    public void setBitmapGenerator(BitmapGenerator generator) {
        mGenerator = generator;
    }

    /**
     * Creates the default BitmapGenerator to be used to retrieve the state of the generation. This
     * is the default implementation and should only be overridden for tests.
     */
    @VisibleForTesting
    public BitmapGenerator.GeneratorCallBack createBitmapGeneratorCallback() {
        return new BitmapGenerator.GeneratorCallBack() {
            @Override
            public void onCompositorError(@CompositorStatus int status) {
                // TODO(tgupta): Add metrics logging here.
                if (status == CompositorStatus.STOPPED_DUE_TO_MEMORY_PRESSURE
                        || status == CompositorStatus.SKIPPED_DUE_TO_MEMORY_PRESSURE) {
                    updateStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else {
                    updateStatus(EntryStatus.GENERATION_ERROR);
                }
            }

            @Override
            public void onCaptureError(@Status int status) {
                // TODO(tgupta): Add metrics logging here.
                if (status == Status.LOW_MEMORY_DETECTED) {
                    updateStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else {
                    updateStatus(EntryStatus.GENERATION_ERROR);
                }
            }

            @Override
            public void onBitmapGenerated(Bitmap bitmap) {
                // TODO(tgupta): Add metrics logging here.
                mGeneratedBitmap = bitmap;
                updateStatus(EntryStatus.BITMAP_GENERATED);
            }
        };
    }

    public void destroy() {
        if (mGenerator != null) {
            mGenerator.destroy();
            mGenerator = null;
        }
        mGeneratedBitmap = null;
    }
}
