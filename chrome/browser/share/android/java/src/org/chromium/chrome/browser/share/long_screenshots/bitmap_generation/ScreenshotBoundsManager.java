// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.paint_preview.mojom.ClipCoordOverride;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Responsible for calculating and tracking the bounds of the capture and composited LongScreenshot
 * Entries.
 */
@NullMarked
public class ScreenshotBoundsManager {
    private static final int NUM_VIEWPORTS_CAPTURE = 10;
    private static final int NUM_VIEWPORTS_CAPTURE_ABOVE_FOR_FULL_CAPTURE = 2;
    private static final int NUM_VIEWPORTS_CAPTURE_BELOW_FOR_FULL_CAPTURE = 4;

    private final Tab mTab;
    private Rect mCaptureRect;
    private @Nullable Size mContentSize;
    private @Nullable Point mScrollOffset;
    private int mClipHeightScaled;

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab Tab to generate the bitmap for.
     */
    public ScreenshotBoundsManager(Context context, Tab tab) {
        mTab = tab;
        calculateClipHeightScaled(context);
        calculateCaptureBounds();
    }

    /** For testing only. */
    private ScreenshotBoundsManager(Tab tab, int clipHeight) {
        mTab = tab;
        mClipHeightScaled = clipHeight;
        calculateCaptureBounds();
    }

    /**
     * To only be used for testing purposes.
     *
     * @param tab Tab to generate the bitmap for.
     * @param clipHeight The height of the device.
     * @return an instance of ScreenshotBoundsManager.
     */
    public static ScreenshotBoundsManager createForTests(
            Context unused_context, Tab tab, int clipHeight) {
        return new ScreenshotBoundsManager(tab, clipHeight);
    }

    /** Calculates the height of the phone used to determine the height of the bitmaps. */
    private void calculateClipHeightScaled(Context context) {
        DisplayAndroid displayAndroid = DisplayAndroid.getNonMultiDisplay(context);
        RenderCoordinates coords =
                RenderCoordinates.fromWebContents(assumeNonNull(mTab.getWebContents()));
        // mClipHeight should be in renderer physical coordinates as this is the coordinate system
        // in which the capture takes place. We want mClipHeight to represent the height of one
        // viewport in the physical coordinate system so we need to divide the display height by the
        // minimum page scale factor in order to get into the physical coordinate space.
        mClipHeightScaled =
                (int)
                        Math.floor(
                                displayAndroid.getDisplayHeight() / coords.getMinPageScaleFactor());
    }

    /** Defines the bounds of the capture. */
    private void calculateCaptureBounds() {
        // We subtract -1 from the bottom so mCaptureRect.height() will be a multiple of
        // mClipHeightScaled.
        mCaptureRect = new Rect(0, 0, 0, mClipHeightScaled * NUM_VIEWPORTS_CAPTURE - 1);
    }

    /**
     * @return The bounds of the capture.
     */
    public Rect getCaptureBounds() {
        return mCaptureRect;
    }

    /**
     * @return The clip X coordinate override of the capture.
     */
    public @ClipCoordOverride.EnumType int getClipXCoordinateOverride() {
        return ClipCoordOverride.NONE;
    }

    /**
     * @return The clip Y coordinate override of the capture.
     */
    public @ClipCoordOverride.EnumType int getClipYCoordinateOverride() {
        // This will capture 1/2 NUM_VIEWPORTS_CAPTURE above and below the scroll offset if
        // possible. If the amount above or below would exceed the document bounds, this will
        // clamp the capture to the start/end and extend in the direction with remaining room.
        return ClipCoordOverride.CENTER_ON_SCROLL_OFFSET;
    }

    /** Sets the composited rect. */
    public void setCompositedSize(@Nullable Size size) {
        mContentSize = size;
    }

    /** Set the composited scroll offset. */
    public void setCompositedScrollOffset(@Nullable Point offset) {
        mScrollOffset = offset;
    }

    /**
     * Defines the bounds of the capture and compositing. Only the starting height is needed. The
     * entire width is always captured.
     *
     * @param yAxisRef Where on the scrolled page the capture and compositing should start.
     */
    public @Nullable Rect calculateClipBoundsAbove(int yAxisRef) {
        if (yAxisRef <= 0) {
            // Already at the top of the capture
            return null;
        }

        int startYAxis = Math.max(yAxisRef - mClipHeightScaled, 0);
        return new Rect(0, startYAxis, 0, yAxisRef);
    }

    /**
     * Defines the bounds of the capture and compositing. Only the starting height is needed. The
     * entire width is always captured.
     *
     * @param yAxisRef Where on the scrolled page the capture and compositing should start.
     */
    public @Nullable Rect calculateClipBoundsBelow(int yAxisRef) {
        assert mContentSize != null;
        if (yAxisRef >= mContentSize.getHeight()) {
            // Already at the bottom of the capture
            return null;
        }

        int endYAxis = Math.min(yAxisRef + mClipHeightScaled, mContentSize.getHeight());
        return new Rect(0, yAxisRef, 0, endYAxis);
    }

    /**
     * Gets the bounds of the entire page for single-bitmap mode.
     * @return the compositing bounds of the full entry.
     */
    public Rect getFullEntryBounds() {
        assert mContentSize != null;
        assert mScrollOffset != null;
        final int totalHeight =
                mClipHeightScaled
                        * (NUM_VIEWPORTS_CAPTURE_ABOVE_FOR_FULL_CAPTURE
                                + NUM_VIEWPORTS_CAPTURE_BELOW_FOR_FULL_CAPTURE
                                + 1);

        int endYAxis = 0;
        int startYAxis =
                mScrollOffset.y - mClipHeightScaled * NUM_VIEWPORTS_CAPTURE_ABOVE_FOR_FULL_CAPTURE;
        startYAxis = Math.max(startYAxis, 0);
        if (startYAxis == 0) {
            // If there isn't enough space above, give any extra to the space to below.
            endYAxis = Math.min(totalHeight, mContentSize.getHeight());
        } else {
            endYAxis =
                    mScrollOffset.y
                            + mClipHeightScaled
                                    * (NUM_VIEWPORTS_CAPTURE_BELOW_FOR_FULL_CAPTURE + 1);
            endYAxis = Math.min(endYAxis, mContentSize.getHeight());
            if (endYAxis == mContentSize.getHeight()) {
                // If there isn't enough space below, give any extra space to above.
                startYAxis = Math.max(mContentSize.getHeight() - totalHeight, 0);
            }
        }
        return new Rect(0, startYAxis, 0, endYAxis);
    }

    /**
     * Calculates the scale factor to be used for bitmaps based on the composited width of the frame
     * at default scale.
     *
     * @return the scale factor to be used for generating bitmaps.
     */
    public float getBitmapScaleFactor() {
        if (mTab.getWebContents() == null || mContentSize == null || mContentSize.getWidth() == 0) {
            // If the web contents crashes/vanished during capture then assume 1f.
            return 1f;
        }

        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());
        return coords.getLastFrameViewportWidthPixInt() / (float) mContentSize.getWidth();
    }
}
