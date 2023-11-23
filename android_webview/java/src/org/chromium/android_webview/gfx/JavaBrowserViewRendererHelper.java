// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Bitmap;
import android.graphics.Canvas;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Provides auxiliary methods related to Picture objects and native SkPictures. */
@JNINamespace("android_webview")
public class JavaBrowserViewRendererHelper {
    private static final String LOGTAG = "JavaBrowserViewRendererHelper";

    /**
     * Provides a Bitmap object with a given width and height used for auxiliary rasterization.
     * |canvas| is optional and if supplied indicates the Canvas that this Bitmap will be
     * drawn into. Note the Canvas will not be modified in any way.
     */
    @CalledByNative
    private static Bitmap createBitmap(int width, int height, Canvas canvas) {
        if (canvas != null) {
            // When drawing into a Canvas, there is a maximum size imposed
            // on Bitmaps that can be drawn. Respect that limit.
            width = Math.min(width, canvas.getMaximumBitmapWidth());
            height = Math.min(height, canvas.getMaximumBitmapHeight());
        }
        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            android.util.Log.e(LOGTAG, "Error allocating bitmap");
        }
        return bitmap;
    }

    /**
     * Draws a provided bitmap into a canvas.
     * Used for convenience from the native side and other static helper methods.
     */
    @CalledByNative
    private static void drawBitmapIntoCanvas(
            Bitmap bitmap, Canvas canvas, int scrollX, int scrollY) {
        canvas.translate(scrollX, scrollY);
        canvas.drawBitmap(bitmap, 0, 0, null);
    }

    // Should never be instantiated.
    private JavaBrowserViewRendererHelper() {}
}
