// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Responsible for calculating and tracking the bounds of the capture and composited
 * LongScreenshot Entries.
 */
public class ScreenshotBoundsManager {
    private static final int NUM_VIEWPORTS_CAPTURE_ABOVE = 4;
    private static final int NUM_VIEWPORTS_CAPTURE_BELOW = 6;

    private Tab mTab;
    private Rect mCaptureRect;
    private int mClipHeightScaled;
    private int mCurrYStartPosition;

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab Tab to generate the bitmap for.
     */
    public ScreenshotBoundsManager(Context context, Tab tab) {
        mTab = tab;
        calculateClipHeightScaled(context);
        calculateCaptureBounds();
    }

    private ScreenshotBoundsManager(Context context, Tab tab, int clipHeight) {
        mTab = tab;
        mClipHeightScaled = clipHeight;
        calculateCaptureBounds();
    }

    /**
     * To only be used for testing purposes.
     *
     * @param context An instance of current Android {@link Context}.
     * @param tab Tab to generate the bitmap for.
     * @param clipHeight The height of the device.
     * @return an instance of ScreenshotBoundsManager.
     */
    @VisibleForTesting
    public static ScreenshotBoundsManager createForTests(Context context, Tab tab, int clipHeight) {
        return new ScreenshotBoundsManager(context, tab, clipHeight);
    }

    /**
     * Calculates the height of the phone used to determine the height of the bitmaps.
     */
    private void calculateClipHeightScaled(Context context) {
        DisplayAndroid displayAndroid = DisplayAndroid.getNonMultiDisplay(context);
        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());
        mClipHeightScaled =
                (int) Math.floor(displayAndroid.getDisplayHeight() * coords.getPageScaleFactor());
    }

    /**
     * Defines the bounds of the capture.
     */
    private void calculateCaptureBounds() {
        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());

        int pageScaleFactor =
                coords.getPageScaleFactorInt() == 0 ? 1 : coords.getPageScaleFactorInt();
        // The current position the user has scrolled to.
        mCurrYStartPosition = coords.getScrollYPixInt() / pageScaleFactor;

        int startYAxis = mCurrYStartPosition - (NUM_VIEWPORTS_CAPTURE_ABOVE * mClipHeightScaled);
        startYAxis = startYAxis < 0 ? 0 : startYAxis;

        int endYAxis = mCurrYStartPosition + (NUM_VIEWPORTS_CAPTURE_BELOW * mClipHeightScaled);
        int maxY = coords.getContentHeightPixInt() / pageScaleFactor;
        endYAxis = endYAxis > maxY ? maxY : endYAxis;

        mCaptureRect = new Rect(0, startYAxis, 0, endYAxis);
    }

    /**
     * @return The bounds of the capture.
     */
    public Rect getCaptureBounds() {
        return mCaptureRect;
    }

    /**
     * Gets the bounds of the initial entry to be used for compositing.
     * @return the compositing bounds of the first entry.
     */
    public Rect getInitialEntryBounds() {
        return calculateClipBoundsBelow(mCurrYStartPosition);
    }

    /**
     * Defines the bounds of the capture and compositing. Only the starting height is needed. The
     * entire width is always captured.
     *
     * @param yAxisRef Where on the scrolled page the capture and compositing should start.
     */
    public Rect calculateClipBoundsAbove(int yAxisRef) {
        if (yAxisRef <= mCaptureRect.top) {
            // Already at the top of the capture
            return null;
        }

        int startYAxis = yAxisRef - mClipHeightScaled;
        startYAxis = startYAxis < 0 ? 0 : startYAxis;
        startYAxis = startYAxis < mCaptureRect.top ? mCaptureRect.top : startYAxis;

        return new Rect(0, startYAxis, 0, yAxisRef);
    }

    /**
     * Defines the bounds of the capture and compositing. Only the starting height is needed. The
     * entire width is always captured.
     *
     * @param yAxisRef Where on the scrolled page the capture and compositing should start.
     */
    public Rect calculateClipBoundsBelow(int yAxisRef) {
        if (yAxisRef >= mCaptureRect.bottom) {
            // Already at the bottom of the capture
            return null;
        }

        // TODO(tgupta): Address the case where the Y axis supersedes the length of the page.
        int endYAxis = yAxisRef + mClipHeightScaled;
        endYAxis = endYAxis > mCaptureRect.bottom ? mCaptureRect.bottom : endYAxis;
        return new Rect(0, yAxisRef, 0, endYAxis);
    }

    /**
     * Calculates the bounds passed in relative to the bounds of the capture. Since 6x the viewport
     * size is captured, the composite bounds needs to be adjusted to be relative to the captured
     * page. For example, let's say that the top Y-axis of the capture rectangle is 100 relative to
     * the top of the website. The Y-axis of the composite rectangle is 150 relative to the top of
     * the website. Then the relative top Y-axis to be used for compositing should be 50 where the
     * top is assumed to the top of the capture.
     *
     * @param compositeRect The bounds relative to the webpage
     * @return The bounds relative to the capture.
     */
    public Rect calculateBoundsRelativeToCapture(Rect compositeRect) {
        int startY = compositeRect.top - mCaptureRect.top;
        startY = (startY < 0) ? 0 : startY;

        int endY = compositeRect.bottom - mCaptureRect.top;
        endY = (endY > mCaptureRect.height()) ? mCaptureRect.height() : endY;

        return new Rect(0, startY, 0, endY);
    }
}
