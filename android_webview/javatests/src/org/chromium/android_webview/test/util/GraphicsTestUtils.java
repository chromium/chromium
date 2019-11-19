// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Graphics-related test utils.
 */
public class GraphicsTestUtils {
    public static float dipScaleForContext(Context context) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return DisplayAndroid.getNonMultiDisplay(context).getDipScale(); });
    }

    /**
     * Draws the supplied {@link AwContents} into the returned {@link Bitmap}.
     *
     * @param awContents The contents to draw
     * @param width The width of the bitmap
     * @param height The height of the bitmap
     */
    public static Bitmap drawAwContents(AwContents awContents, int width, int height) {
        return doDrawAwContents(awContents, width, height, null, null);
    }

    public static Bitmap drawAwContentsOnUiThread(
            final AwContents awContents, final int width, final int height) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> drawAwContents(awContents, width, height));
    }

    /**
     * Draws the supplied {@link AwContents} after applying a translate into the returned
     * {@link Bitmap}.
     *
     * @param awContents The contents to draw
     * @param width The width of the bitmap
     * @param height The height of the bitmap
     * @param dx The distance to translate in X
     * @param dy The distance to translate in Y
     */
    public static Bitmap drawAwContents(
            AwContents awContents, int width, int height, float dx, float dy) {
        return doDrawAwContents(awContents, width, height, dx, dy);
    }

    /**
     * Draws the supplied {@link View} into the returned {@link Bitmap}.
     *
     * @param view The view to draw
     * @param width The width of the bitmap
     * @param height The height of the bitmap
     */
    public static Bitmap drawView(View view, int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        view.draw(canvas);
        return bitmap;
    }

    public static int sampleBackgroundColorOnUiThread(final AwContents awContents)
            throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> drawAwContents(awContents, 10, 10, 0, 0).getPixel(0, 0));
    }

    // Gets the pixel color at the center of AwContents.
    public static int getPixelColorAtCenterOfView(
            final AwContents awContents, final AwTestContainerView testContainerView) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> drawAwContents(awContents, 2, 2,
                                -(float) testContainerView.getWidth() / 2,
                                -(float) testContainerView.getHeight() / 2)
                                   .getPixel(0, 0));
    }

    public static void pollForBackgroundColor(final AwContents awContents, final int c) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> sampleBackgroundColorOnUiThread(awContents) == c);
    }

    private static Bitmap doDrawAwContents(
            AwContents awContents, int width, int height, Float dx, Float dy) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        if (dx != null && dy != null) {
            canvas.translate(dx, dy);
        }
        awContents.onDraw(canvas);
        return bitmap;
    }
}
