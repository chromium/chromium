// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;

import androidx.annotation.Px;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;

/** Helper methods to treat Autofill images. */
final class AutofillImageFetcherUtils {
    private AutofillImageFetcherUtils() {}

    /**
     * Converts dp units to pixel units.
     *
     * @param dpDimensionId Resource id of the dimension in dp.
     * @return Converted dimension in pixels.
     */
    static @Px int getPixelSize(int dpDimensionId) {
        return ContextUtils.getApplicationContext()
                .getResources()
                .getDimensionPixelSize(dpDimensionId);
    }

    /**
     * Treats Pix account image as per specifications:
     *
     * <p>Resizes the logo to 18dp x 18dp if the logo is of a different dimension, and adds a corner
     * radius of 3dp.
     *
     * <p>Center aligns the logo in a white background of 40dp x 24dp with a corner radius of 3dp.
     *
     * <p>Applies a grey border of thickness 1dp around the composite image.
     *
     * <p>Note: Since an average user is expected to have single digit number of accounts,
     * micro-optimizations aren't applied to keep the function simple.
     *
     * @param bitmap The input bitmap of size 18dp x 18dp.
     * @return Bitmap with enhancements of size 40dp x 24dp.
     */
    static Bitmap treatPixAccountImage(Bitmap bitmap) {
        @Px int logoSize = getPixelSize(R.dimen.square_card_icon_side_length);
        @Px int logoCornerRadius = getPixelSize(R.dimen.square_card_icon_corner_radius);
        @Px int iconWidth = getPixelSize(R.dimen.large_card_icon_width);
        @Px int iconHeight = getPixelSize(R.dimen.large_card_icon_height);
        @Px int iconCornerRadius = getPixelSize(R.dimen.large_card_icon_corner_radius);
        @Px int iconBorderWidth = getPixelSize(R.dimen.card_icon_border_width);

        if (bitmap.getWidth() != logoSize || bitmap.getHeight() != logoSize) {
            bitmap = Bitmap.createScaledBitmap(bitmap, logoSize, logoSize, /* filter= */ true);
        }

        // Round the corners of the square bitmap.
        Bitmap squareBitmap = Bitmap.createBitmap(logoSize, logoSize, Bitmap.Config.ARGB_8888);
        Canvas squareCanvas = new Canvas(squareBitmap);
        Paint squarePaint = new Paint();
        squarePaint.setAntiAlias(true);
        RectF squareRectF = new RectF(0, 0, logoSize, logoSize);
        squareCanvas.drawRoundRect(squareRectF, logoCornerRadius, logoCornerRadius, squarePaint);
        squarePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        squareCanvas.drawBitmap(bitmap, 0, 0, squarePaint);

        // Create a white background and place the square logo in the center.
        Bitmap backgroundBitmap =
                Bitmap.createBitmap(iconWidth, iconHeight, Bitmap.Config.ARGB_8888);
        Canvas backgroundCanvas = new Canvas(backgroundBitmap);
        Paint backgroundPaint = new Paint();
        backgroundPaint.setColor(Color.WHITE);
        backgroundPaint.setAntiAlias(true);
        backgroundCanvas.drawRect(0, 0, iconWidth, iconHeight, backgroundPaint);
        int left = (iconWidth - logoSize) / 2;
        int top = (iconHeight - logoSize) / 2;
        backgroundCanvas.drawBitmap(squareBitmap, left, top, null);

        // Round the corners of the composite image.
        Bitmap bitmapWithEnhancements =
                Bitmap.createBitmap(iconWidth, iconHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmapWithEnhancements);
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        Rect rect = new Rect(0, 0, iconWidth, iconHeight);
        RectF rectF = new RectF(rect);
        canvas.drawRoundRect(rectF, iconCornerRadius, iconCornerRadius, paint);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(backgroundBitmap, rect, rect, paint);

        // Add the grey border.
        // TODO(crbug.com/389947287): Verify the border color.
        int greyColor =
                ContextCompat.getColor(
                        ContextUtils.getApplicationContext(), R.color.baseline_neutral_90);
        paint.setColor(greyColor);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(iconBorderWidth);
        canvas.drawRoundRect(rectF, iconCornerRadius, iconCornerRadius, paint);

        return bitmapWithEnhancements;
    }
}
