// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
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
            EntryStatus.BITMAP_GENERATED, EntryStatus.CAPTURE_COMPLETE,
            EntryStatus.CAPTURE_IN_PROGRESS, EntryStatus.BITMAP_GENERATION_IN_PROGRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryStatus {
        int UNKNOWN = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int BITMAP_GENERATED = 3;
        int CAPTURE_COMPLETE = 4;
        int CAPTURE_IN_PROGRESS = 5;
        int BITMAP_GENERATION_IN_PROGRESS = 6;
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
     * @param context An instance of current Android {@link Context}.
     * @param tab The tab to capture the results for.
     * @param yAxisRef Y-axis reference used to calculate the coordinates of the bitmap to generate.
     * @param clipHeight Height of the capture.
     * @param generator BitmapGenerator to be used to capture and composite the website.
     * @param generatingAbove Whether to use the yAxisRef as the top (generatingAbove = false) or
     *            bottom yAxis coordinate (generatingAbove = true);
     */
    public LongScreenshotsEntry(Context context, Tab tab, int yAxisRef, int clipHeight,
            BitmapGenerator generator, boolean generatingAbove) {
        mContext = context;
        mTab = tab;
        calculateClipBounds(yAxisRef, clipHeight, generatingAbove);
        mGenerator = generator;
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
        return mRect.top;
    }

    public int getEndYAxis() {
        return mRect.bottom;
    }

    public void generateBitmap() {
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

    /**
     * Defines the bounds of the capture and compositing. Only the starting height and the height of
     * the clip is needed. The entire width is always captured.
     *
     * @param yAxisRef Where on the scrolled page the capture and compositing should start.
     * @param clipHeight The length of the webpage that should be captured.
     * @param generatingAbove Whether to use the yAxisRef as the top (generatingAbove = false) or
     *            bottom yAxis coordinate (generatingAbove = true);
     */
    private void calculateClipBounds(int yAxisRef, int clipHeight, boolean generatingAbove) {
        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());

        int startYAxis;
        int endYAxis;
        int clipHeightScaled = (int) (clipHeight * coords.getPageScaleFactor());
        if (generatingAbove) {
            endYAxis = yAxisRef;
            startYAxis = yAxisRef - clipHeightScaled;
            startYAxis = startYAxis < 0 ? 0 : startYAxis;
        } else {
            startYAxis = yAxisRef;
            // TODO(tgupta): Address the case where the Y axis supersedes the length of the page.
            endYAxis = startYAxis + clipHeightScaled;
        }

        int clipWidth =
                (int) Math.floor(coords.getContentWidthPixInt() / coords.getPageScaleFactor());

        mRect = new Rect(0, startYAxis, 0, endYAxis);
    }

    @VisibleForTesting
    public void setBitmapGenerator(BitmapGenerator generator) {
        mGenerator = generator;
    }

    private void onBitmapGenerated(Bitmap bitmap) {
        // TODO(tgupta): Add metrics logging here.
        mGeneratedBitmap = bitmap;
        updateStatus(EntryStatus.BITMAP_GENERATED);
    }

    private void onBitmapGenerationError() {
        updateStatus(EntryStatus.GENERATION_ERROR);
    }

    public void updateStatus(@EntryStatus int status) {
        mCurrentStatus = status;
        if (mEntryListener != null) {
            mEntryListener.onResult(mCurrentStatus);
        }
    }

    public void destroy() {
        if (mGenerator != null) {
            mGenerator.destroy();
            mGenerator = null;
        }
        mGeneratedBitmap = null;
    }
}
