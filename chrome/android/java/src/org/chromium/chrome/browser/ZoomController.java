// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;

import android.os.SystemClock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.accessibility.AccessibilityFeatureMap;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.zoom.ZoomConstants;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.GestureEventType;

/**
 * Handles zoom in/out WebContents.
 * <p>The methods use the term 'zoom' for legacy reasons, but relates
 * to what chrome calls the 'page scale factor'.
 */
@NullMarked
public class ZoomController {
    /**
     * Zooms in the WebContents by 25% (or less if that would result in zooming in more than
     * possible).
     *
     * @param webContents {@link WebContents} to zoom in.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomIn(@Nullable WebContents webContents) {
        if (!AccessibilityFeatureMap.sAndroidZoomIndicator.isEnabled()) {
            return pinchByDelta(webContents, ZoomConstants.ZOOM_IN_DELTA);
        }
        return changeZoomLevel(webContents, /* zoomIn= */ false);
    }

    /**
     * Zooms out the WebContents by 20% (or less if that would result in zooming out more than
     * possible).
     *
     * @param webContents {@link WebContents} to zoom out.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomOut(@Nullable WebContents webContents) {
        if (!AccessibilityFeatureMap.sAndroidZoomIndicator.isEnabled()) {
            return pinchByDelta(webContents, ZoomConstants.ZOOM_OUT_DELTA);
        }
        return changeZoomLevel(webContents, /* zoomIn= */ true);
    }

    /**
     * Resets the zoom factor of the WebContents.
     *
     * @param webContents {@link WebContents} to reset the zoom of.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomReset(
            @Nullable WebContents webContents,
            @Nullable BrowserContextHandle browserContextHandle) {
        if (!AccessibilityFeatureMap.sAndroidZoomIndicator.isEnabled()) {
            return pinchByDelta(webContents, ZoomConstants.ZOOM_RESET_DELTA);
        }
        if (webContents == null || browserContextHandle == null) return false;
        double defaultZoomFactor = HostZoomMap.getDefaultZoomLevel(browserContextHandle);
        HostZoomMap.setZoomLevel(webContents, defaultZoomFactor);
        return true;
    }

    private static boolean pinchByDelta(@Nullable WebContents webContents, float delta) {
        if (webContents == null) return false;
        EventForwarder eventForwarder = webContents.getEventForwarder();
        long timeMs = SystemClock.uptimeMillis();
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BEGIN, timeMs, 0.f);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BY, timeMs, delta);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_END, timeMs, 0.f);
        return true;
    }

    private static boolean changeZoomLevel(@Nullable WebContents webContents, boolean zoomIn) {
        if (webContents == null) return false;
        double currentZoomFactor = HostZoomMap.getZoomLevel(webContents);
        int index = PageZoomUtils.getNextIndex(zoomIn, currentZoomFactor);

        if (index >= 0) {
            snapToIndex(index, webContents);
        }
        return true;
    }

    private static void snapToIndex(int index, WebContents webContents) {
        double newZoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        HostZoomMap.setZoomLevel(webContents, newZoomFactor);
    }
}
