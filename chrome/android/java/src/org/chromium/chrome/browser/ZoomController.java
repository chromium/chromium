// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.SystemClock;

import org.chromium.components.zoom.ZoomConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.GestureEventType;

/**
 * Handles zoom in/out WebContents.
 * <p>The methods use the term 'zoom' for legacy reasons, but relates
 * to what chrome calls the 'page scale factor'.
 */
public class ZoomController {
    /**
     * Zooms in the WebContents by 25% (or less if that would result in
     * zooming in more than possible).
     *
     * @param webContents {@link WebContents} to zoom in.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomIn(WebContents webContents) {
        return pinchByDelta(webContents, ZoomConstants.ZOOM_IN_DELTA);
    }

    /**
     * Zooms out the WebContents by 20% (or less if that would result in
     * zooming out more than possible).
     *
     * @param webContents {@link WebContents} to zoom out.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomOut(WebContents webContents) {
        return pinchByDelta(webContents, ZoomConstants.ZOOM_OUT_DELTA);
    }

    /**
     * Resets the zoom factor of the WebContents.
     *
     * @param webContents {@link WebContents} to reset the zoom of.
     * @return True if there was a zoom change, false otherwise.
     */
    public static boolean zoomReset(WebContents webContents) {
        // Negative value to reset zoom level.
        return pinchByDelta(webContents, ZoomConstants.ZOOM_RESET_DELTA);
    }

    private static boolean pinchByDelta(WebContents webContents, float delta) {
        if (webContents == null) return false;
        EventForwarder eventForwarder = webContents.getEventForwarder();
        long timeMs = SystemClock.uptimeMillis();
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BEGIN, timeMs, 0.f);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BY, timeMs, delta);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_END, timeMs, 0.f);
        return true;
    }
}
