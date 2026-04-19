// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.graphics.Rect;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/** Bridge to access C++ PictureInPictureBoundsCache from Java. */
@NullMarked
@JNINamespace("picture_in_picture")
public final class PictureInPictureBoundsCacheBridge {

    private PictureInPictureBoundsCacheBridge() {}

    /**
     * Returns the cached window bounds to use for a new PiP window.
     *
     * @param webContents The WebContents of the opener tab.
     * @param openerDisplayId The ID of the display where the opener tab is located.
     * @param requestedWidth The width requested by the site, or a negative value if not specified.
     * @param requestedHeight The height requested by the site, or a negative value if not
     *     specified.
     * @return The cached bounds in pixels, or null if no cache matches.
     */
    public static @Nullable Rect getBoundsForNewWindow(
            WebContents webContents, int openerDisplayId, int requestedWidth, int requestedHeight) {
        int @Nullable [] coords =
                PictureInPictureBoundsCacheBridgeJni.get()
                        .getBoundsForNewWindow(
                                webContents, openerDisplayId, requestedWidth, requestedHeight);
        if (coords == null) {
            return null;
        }
        assert coords.length == 4 : "Coords array must have length 4";
        return new Rect(coords[0], coords[1], coords[2], coords[3]);
    }

    /**
     * Updates the cached bounds for the given WebContents.
     *
     * @param webContents The WebContents of the opener tab.
     * @param mostRecentBounds The most recent bounds of the PiP window in pixels.
     * @param openerDisplayId The ID of the display where the opener tab is located.
     * @param pipDisplayId The ID of the display where the PiP window is located.
     */
    public static void updateCachedBounds(
            WebContents webContents, Rect mostRecentBounds, int openerDisplayId, int pipDisplayId) {
        PictureInPictureBoundsCacheBridgeJni.get()
                .updateCachedBounds(
                        webContents,
                        mostRecentBounds.left,
                        mostRecentBounds.top,
                        mostRecentBounds.right,
                        mostRecentBounds.bottom,
                        openerDisplayId,
                        pipDisplayId);
    }

    /**
     * Clears the cached bounds for the given WebContents.
     *
     * @param webContents The WebContents of the opener tab.
     */
    public static void clearCachedBounds(WebContents webContents) {
        PictureInPictureBoundsCacheBridgeJni.get().clearCachedBounds(webContents);
    }

    @NativeMethods
    public interface Natives {
        int @Nullable [] getBoundsForNewWindow(
                @JniType("content::WebContents*") WebContents webContents,
                int openerDisplayId,
                int requestedWidth,
                int requestedHeight);

        void updateCachedBounds(
                @JniType("content::WebContents*") WebContents webContents,
                int left,
                int top,
                int right,
                int bottom,
                int openerDisplayId,
                int pipDisplayId);

        void clearCachedBounds(@JniType("content::WebContents*") WebContents webContents);
    }
}
