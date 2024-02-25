// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.os.Build;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.shell_apk.R;
import org.chromium.webapk.shell_apk.WebApkUtils;

/** Contains splash screen related utility methods. */
public class SplashUtils {
    /**
     * The maximum image size to PNG-encode. JPEG encoding is > 2 times faster on large bitmaps.
     * JPEG encoding is preferable to downscaling the screenshot.
     */
    private static final int MAX_SIZE_ENCODE_PNG = 1024 * 1024;

    /** Helper class for the return type of {@link #createScaledBitmapAndCanvas}. */
    static class BitmapAndCanvas {
        public final Bitmap bitmap;
        public final Canvas canvas;

        private BitmapAndCanvas(Bitmap bitmap, Canvas canvas) {
            this.bitmap = bitmap;
            this.canvas = canvas;
        }
    }

    /** Creates view with splash screen. */
    public static View createSplashView(Context context) {
        Resources resources = context.getResources();
        Bitmap icon = WebApkUtils.decodeBitmapFromDrawable(resources, R.drawable.splash_icon);
        int backgroundColor =
                WebApkUtils.inDarkMode(context)
                        ? WebApkUtils.getColor(resources, R.color.dark_background_color_non_empty)
                        : WebApkUtils.getColor(resources, R.color.background_color_non_empty);

        FrameLayout layout = new FrameLayout(context);
        SplashLayout.createLayout(
                context,
                layout,
                icon,
                WebApkUtils.isSplashIconAdaptive(context),
                /* isIconGenerated= */ false,
                resources.getString(R.string.name),
                WebApkUtils.shouldUseLightForegroundOnBackground(backgroundColor));
        layout.setBackgroundColor(backgroundColor);
        return layout;
    }

    /**
     * Returns bitmap with screenshot of passed-in view. Downsamples screenshot so that it is no
     * more than {@maxSizeInBytes}.
     */
    public static Bitmap screenshotView(View view, int maxSizeBytes) {
        // Implementation copied from Android shared element code -
        // TransitionUtils#createViewBitmap().

        int width = view.getWidth();
        int height = view.getHeight();
        if (width == 0 || height == 0) return null;

        BitmapAndCanvas pair = createScaledBitmapAndCanvas(width, height, maxSizeBytes);

        view.draw(pair.canvas);
        return pair.bitmap;
    }

    /** Creates a Bitmap of at most {@code maxSizeBytes} with an attached Canvas to draw on it. */
    static BitmapAndCanvas createScaledBitmapAndCanvas(int width, int height, int maxSizeBytes) {
        float scale = Math.min(1f, ((float) maxSizeBytes) / (4 * width * height));
        width = Math.round(width * scale);
        height = Math.round(height * scale);

        Matrix matrix = new Matrix();
        matrix.postScale(scale, scale);

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.concat(matrix);

        return new BitmapAndCanvas(bitmap, canvas);
    }

    /** Selects encoding for the bitmap based on its size. */
    public static Bitmap.CompressFormat selectBitmapEncoding(int width, int height) {
        return (width * height <= MAX_SIZE_ENCODE_PNG)
                ? Bitmap.CompressFormat.PNG
                : Bitmap.CompressFormat.JPEG;
    }

    /** Creates splash view with the passed-in dimensions and screenshots it. */
    public static Bitmap createAndImmediatelyScreenshotSplashView(
            Context context, int splashWidth, int splashHeight, int maxSizeBytes) {
        if (splashWidth <= 0 || splashHeight <= 0) return null;

        View splashView =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                        ? SplashUtilsForS.createSplashView(context)
                        : createSplashView(context);

        splashView.measure(
                View.MeasureSpec.makeMeasureSpec(splashWidth, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(splashHeight, View.MeasureSpec.EXACTLY));
        splashView.layout(0, 0, splashWidth, splashHeight);
        return screenshotView(splashView, maxSizeBytes);
    }
}
