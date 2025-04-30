// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** Helper methods to treat Autofill images. */
@NullMarked
public final class AutofillImageFetcherUtils {
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
     * Adds corner radius to a bitmap.
     *
     * @param bitmap The input bitmap whose corners are to be rounded.
     * @param cornerRadius Corner radius in Px.
     * @return A copy of the input bitmap with rounded corners.
     */
    static Bitmap roundCorners(Bitmap bitmap, @Px int cornerRadius) {
        Bitmap outputBitmap =
                Bitmap.createBitmap(bitmap.getWidth(), bitmap.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(outputBitmap);
        Path path = new Path();
        RectF rect = new RectF(0, 0, bitmap.getWidth(), bitmap.getHeight());
        path.addRoundRect(rect, cornerRadius, cornerRadius, Path.Direction.CW);
        canvas.clipPath(path);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        canvas.drawBitmap(bitmap, 0, 0, paint);

        return outputBitmap;
    }

    /**
     * Adds a background and center aligns the input bitmap.
     *
     * <p>The input bitmap dimensions should not be larger than the background dimensions. If it is
     * larger, the input bitmap will be returned without any modifications.
     *
     * @param bitmap The input bitmap to which the background has to be added.
     * @param backgroundWidth Background width in Px.
     * @param backgroundHeight Background height in Px.
     * @param backgroundColor Background color.
     * @return A new bitmap which is a composite of the input bitmap placed on a background.
     */
    static Bitmap addCenterAlignedBackground(
            Bitmap bitmap, @Px int backgroundWidth, @Px int backgroundHeight, int backgroundColor) {
        if (bitmap.getWidth() > backgroundWidth || bitmap.getHeight() > backgroundHeight) {
            return bitmap;
        }
        Bitmap outputBitmap =
                Bitmap.createBitmap(backgroundWidth, backgroundHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(outputBitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(backgroundColor);
        canvas.drawRect(0, 0, backgroundWidth, backgroundHeight, paint);
        float left = (backgroundWidth - bitmap.getWidth()) / 2f;
        float top = (backgroundHeight - bitmap.getHeight()) / 2f;
        canvas.drawBitmap(bitmap, left, top, /* paint= */ null);

        return outputBitmap;
    }

    /**
     * Adds border to a bitmap.
     *
     * @param bitmap The input to which a border is to be added.
     * @param cornerRadius Corner radius of the bitmap in Px. The border will be added with the same
     *     corner radius.
     * @param borderThickness Border thickness in Px.
     * @param borderColor Border color.
     * @return A copy of the input bitmap with border.
     */
    static Bitmap addBorder(
            Bitmap bitmap, @Px int cornerRadius, @Px int borderThickness, int borderColor) {
        Bitmap outputBitmap = bitmap.copy(assumeNonNull(bitmap.getConfig()), /* isMutable= */ true);
        Canvas canvas = new Canvas(outputBitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(borderColor);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(borderThickness);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        RectF rectF = new RectF(0, 0, bitmap.getWidth(), bitmap.getHeight());
        canvas.drawRoundRect(rectF, cornerRadius, cornerRadius, paint);

        return outputBitmap;
    }

    /**
     * Adds size parameters to a FIFE supported Pix account image asset URL.
     *
     * <p>The image fetched with the formatted URL preserves the original aspect ratio. If the size
     * dimensions do not match the original aspect ratio, the side with the more restrictive size is
     * matched.
     *
     * @param url A FIFE URL to fetch the image.
     * @return {@link GURL} formatted with the required image size.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static GURL getPixAccountImageUrlWithParams(GURL url) {
        @Px int logoSize = getPixelSize(R.dimen.square_card_icon_side_length);
        StringBuilder output = new StringBuilder(url.getSpec());
        output.append("=w").append(logoSize).append("-h").append(logoSize);

        return new GURL(output.toString());
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
        @Px int iconCornerRadius = getPixelSize(R.dimen.large_card_icon_corner_radius);

        if (bitmap.getWidth() != logoSize || bitmap.getHeight() != logoSize) {
            bitmap = Bitmap.createScaledBitmap(bitmap, logoSize, logoSize, /* filter= */ true);
        }

        // Round the corners of the square bitmap.
        Bitmap logoWithRoundCorners =
                roundCorners(bitmap, getPixelSize(R.dimen.square_card_icon_corner_radius));

        // Create a white background and place the square logo in the center.
        Bitmap compositeBitmap =
                addCenterAlignedBackground(
                        logoWithRoundCorners,
                        getPixelSize(R.dimen.large_card_icon_width),
                        getPixelSize(R.dimen.large_card_icon_height),
                        Color.WHITE);

        // Round the corners of the composite bitmap.
        Bitmap compositeBitmapWithRoundCorners = roundCorners(compositeBitmap, iconCornerRadius);

        // Add the grey border.
        int greyColor =
                ContextCompat.getColor(
                        ContextUtils.getApplicationContext(), R.color.baseline_neutral_variant_90);
        Bitmap compositeBitmapWithRoundCornersAndGreyBorder =
                addBorder(
                        compositeBitmapWithRoundCorners,
                        iconCornerRadius,
                        getPixelSize(R.dimen.card_icon_border_width),
                        greyColor);

        return compositeBitmapWithRoundCornersAndGreyBorder;
    }
}
