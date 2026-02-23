// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.DisplayMetrics;

import androidx.annotation.VisibleForTesting;
import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

/** Utility class containing static methods for image matrix calculations. */
@NullMarked
public final class CropImageUtils {
    /**
     * Calculates a new matrix that preserves the visual center point from the other orientation.
     *
     * @param sourceMatrix The matrix from the previous orientation.
     * @param targetViewWidth The width of the view in the new orientation.
     * @param targetViewHeight The height of the view in the new orientation.
     * @param sourceViewWidth The width of the view in the previous orientation.
     * @param sourceViewHeight The height of the view in the previous orientation.
     * @param bitmap The bitmap being displayed.
     * @return A new matrix calculated from the shared center point.
     */
    public static Matrix calculateMatrixFromSharedCenter(
            Matrix sourceMatrix,
            int targetViewWidth,
            int targetViewHeight,
            int sourceViewWidth,
            int sourceViewHeight,
            Bitmap bitmap) {
        Matrix resultMatrix = new Matrix();

        // Step 1: Finds the center of the old screen.
        final float[] screenCenterPoint = {sourceViewWidth / 2f, sourceViewHeight / 2f};

        // Step 2: Translates the screen coordinates from step 1 back to the original bitmap using
        // an inverse matrix. This gives the exact pixel on the image that the user was looking at.
        Matrix inverseSourceMatrix = new Matrix();
        if (sourceMatrix.invert(inverseSourceMatrix)) {
            inverseSourceMatrix.mapPoints(screenCenterPoint);
        } else {
            // This case is very unlikely to happen. If the sourceMatrix can't be inverted
            // (e.g., if its scale is 0 which collapses the entire image down to a single point or a
            // line ), we can't perform the translation. The safest fallback is to simply use
            // the absolute center of the bitmap as our focal point.
            screenCenterPoint[0] = bitmap.getWidth() / 2f;
            screenCenterPoint[1] = bitmap.getHeight() / 2f;
        }

        float bitmapFocalX = screenCenterPoint[0];
        float bitmapFocalY = screenCenterPoint[1];

        // Step 3: Calculate and apply the translation (deltaX, deltaY) required to align the
        // bitmap's focal point (bitmapFocalX, bitmapFocalY) with the center of the new target view.
        float bitmapWidth = bitmap.getWidth();
        float bitmapHeight = bitmap.getHeight();
        float standardScale =
                calculateMinScaleToFill(
                        targetViewWidth, targetViewHeight, bitmapWidth, bitmapHeight);

        // Step 4: Calculate the scale needed to strictly center the focal point on the bitmap.
        float focalScaleX = calculateFocalScale(targetViewWidth, bitmapWidth, bitmapFocalX);
        float focalScaleY = calculateFocalScale(targetViewHeight, bitmapHeight, bitmapFocalY);

        // Step 5: Determine final scale.
        // We take the maximum of all scales to satisfy three conditions:
        // - Image covers the view width.
        // - Image covers the view height.
        // - Image is zoomed in enough to keep the focal point centered without showing edges.
        float scale = Math.max(standardScale, Math.max(focalScaleX, focalScaleY));

        // Step 6: Apply the transformation to center the focal point on the bitmap.
        float deltaX = (targetViewWidth / 2f) - (bitmapFocalX * scale);
        float deltaY = (targetViewHeight / 2f) - (bitmapFocalY * scale);

        resultMatrix.setScale(scale, scale);
        resultMatrix.postTranslate(deltaX, deltaY);
        return resultMatrix;
    }

    /**
     * Corrects the input matrix in terms of the scale and translation to ensure it perfectly fills
     * the given target dimensions, preventing any boundary gaps.
     *
     * <p>This method allocates a new {@code float[9]} array on every call. Do not use this method
     * inside hot loops (like {@code onScroll} or {@code onScale}). Use the overloaded version that
     * accepts a reusable {@code matrixValues} array instead.
     *
     * @param matrixToValidate The matrix to correct. This matrix will be modified in place.
     * @param targetWidth The width of the viewport the matrix must fill.
     * @param targetHeight The height of the viewport the matrix must fill.
     * @param bitmap The bitmap being displayed.
     */
    public static void validateMatrix(
            Matrix matrixToValidate, int targetWidth, int targetHeight, Bitmap bitmap) {
        validateMatrix(matrixToValidate, targetWidth, targetHeight, bitmap, new float[9]);
    }

    /**
     * Adjusts the provided matrix to ensure the underlying bitmap completely covers the target
     * dimensions without leaving any empty boundary space.
     *
     * <p><b>Coordinate Transformation:</b><br>
     * The matrix defines the scale and translation required to map the original bitmap coordinates
     * into the viewport's coordinate space. For any point {@code (x, y)} on the source bitmap, its
     * final mapped position {@code (x', y')} is calculated as:
     *
     * <pre>{@code
     * x' = (x * scale) + transX
     * y' = (y * scale) + transY
     * }</pre>
     *
     * <p>This method enforces two strict constraints:
     *
     * <ol>
     *   <li><b>Minimum Scale:</b> The bitmap is scaled up at least enough to guarantee it spans the
     *       entire width and height of the view.
     *   <li><b>Clamped Translation:</b> The bitmap is shifted so its edges remain flush against or
     *       outside the viewport boundaries, preventing visible gaps.
     * </ol>
     *
     * @param matrixToValidate The matrix to correct. This matrix will be modified in place.
     * @param targetWidth The width of the viewport the matrix must fill.
     * @param targetHeight The height of the viewport the matrix must fill.
     * @param bitmap The source bitmap being displayed.
     * @param matrixValues A pre-allocated float array (length 9) for temporary calculations.
     */
    public static void validateMatrix(
            Matrix matrixToValidate,
            int targetWidth,
            int targetHeight,
            Bitmap bitmap,
            float[] matrixValues) {
        if (targetWidth == 0) return;

        // Step 1: Validate Scale. Ensure the image is big enough
        matrixToValidate.getValues(matrixValues);
        float scale = matrixValues[Matrix.MSCALE_X];

        // Calculates the absolute minimum required zoom.
        float minScale =
                calculateMinScaleToFill(
                        targetWidth, targetHeight, bitmap.getWidth(), bitmap.getHeight());

        // If the current scale is too small, scale it up from the center of the view.
        if (scale < minScale) {
            float scaleCorrection = minScale / scale;
            matrixToValidate.postScale(
                    scaleCorrection, scaleCorrection, targetWidth / 2f, targetHeight / 2f);
        }

        // Step 2: Validate Translation. Ensure the image is positioned correctly.
        matrixToValidate.getValues(matrixValues);
        scale = matrixValues[Matrix.MSCALE_X];

        // `transX` and `transY` are the current top-left coordinates of the scaled bitmap.
        // They represent the current position of the bitmap inside the view.
        float transX = matrixValues[Matrix.MTRANS_X];
        float transY = matrixValues[Matrix.MTRANS_Y];

        // `deltaTransX` and `deltaTransY` represent the required correction needed to bring the
        // bitmap to a valid position. If the current position is valid, these will be 0.
        float deltaTransX =
                calculateTranslationCorrection(transX, scale * bitmap.getWidth(), targetWidth);
        float deltaTransY =
                calculateTranslationCorrection(transY, scale * bitmap.getHeight(), targetHeight);

        // Step 3: Apply the calculated correction if needed.
        if (deltaTransX != 0 || deltaTransY != 0) {
            // The postTranslate call effectively updates the matrix's internal translation.
            // The relationship is: newTransX = currentTransX + deltaTransX.
            matrixToValidate.postTranslate(deltaTransX, deltaTransY);
        }
    }

    /**
     * Calculates the initial "center-crop" matrix that makes the image fill the view.
     *
     * @param matrix The matrix to save the result into.
     * @param viewWidth The current width of the view.
     * @param viewHeight The current height of the view.
     * @param bitmap The bitmap being displayed.
     */
    public static void calculateInitialCenterCropMatrix(
            Matrix matrix, int viewWidth, int viewHeight, Bitmap bitmap) {
        float bitmapWidth = bitmap.getWidth();
        float bitmapHeight = bitmap.getHeight();

        float scale = calculateMinScaleToFill(viewWidth, viewHeight, bitmapWidth, bitmapHeight);

        // Center the image
        float deltaX = (viewWidth - (bitmapWidth * scale)) / 2f;
        float deltaY = (viewHeight - (bitmapHeight * scale)) / 2f;

        matrix.reset();
        matrix.setScale(scale, scale);
        matrix.postTranslate(deltaX, deltaY);
    }

    /**
     * Returns the current window dimensions in pixels.
     *
     * <p><b>Detailed Logics:</b>
     *
     * <ol>
     *   <li><b>Activity Context:</b> If the context provides an {@link Activity}, this method uses
     *       {@link WindowMetricsCalculator} to retrieve the exact window bounds. This correctly
     *       handles multi-window modes, split-screen, and letterboxing.
     *   <li><b>Fallback:</b> If no Activity is found (e.g., Application Context or Service), it
     *       falls back to {@link DisplayMetrics}. Note that in split-screen mode without an
     *       Activity, this returns the physical screen size, which may be larger than the actual
     *       visible area.
     * </ol>
     *
     * @param context The {@link Context} used to retrieve window metrics.
     * @return A {@link Point} where x is the width and y is the height in pixels.
     */
    public static Point getCurrentWindowDimensions(Context context) {
        Activity activity = ContextUtils.activityFromContext(context);

        if (activity != null) {
            WindowMetrics metrics =
                    WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(activity);
            Rect bounds = metrics.getBounds();
            return new Point(bounds.width(), bounds.height());
        }

        // Fallback to DisplayMetrics dimensions
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        return new Point(displayMetrics.widthPixels, displayMetrics.heightPixels);
    }

    /**
     * Calculates the minimum scale factor required for the bitmap to completely fill the target
     * dimensions.
     */
    @VisibleForTesting
    static float calculateMinScaleToFill(int targetW, int targetH, float bitmapW, float bitmapH) {
        float scaleX = (float) targetW / bitmapW;
        float scaleY = (float) targetH / bitmapH;
        return Math.max(scaleX, scaleY);
    }

    /**
     * Calculates the scale required to keep a specific focal point centered on a specific axis
     * without revealing the background edge.
     */
    @VisibleForTesting
    static float calculateFocalScale(
            int targetDimension, float bitmapDimension, float focalPointOnBitmap) {
        // To keep the focal point centered without revealing the background behind the bitmap, the
        // distance from the focal point to the nearest bitmap edge must be scaled to cover at least
        // half of the target view dimension.
        float distanceToEdge = Math.min(focalPointOnBitmap, bitmapDimension - focalPointOnBitmap);

        // A distance of zero means the focal point is exactly on the edge of the bitmap.
        // In this case, it is mathematically impossible to center the focal point without
        // revealing the background, as it would require an infinite scale factor.
        // We must check for this to prevent a division-by-zero error and return 0 indicating that a
        // valid scale could not be computed.
        if (distanceToEdge > 0) {
            return (targetDimension / 2f) / distanceToEdge;
        }
        return 0;
    }

    /**
     * Calculates the required translation adjustment to prevent boundary gaps.
     *
     * <p><b>Visual Explanation of Cases:</b> The following diagrams illustrate the logic for the
     * horizontal (X-axis), but the same logics apply to Y-axis.
     *
     * <pre>
     * <b>Case 1: Image is smaller or equal to the view </b>
     * The goal is to calculate the 'deltaTrans' to move the bitmap to the center of the view.
     *
     * Top left of the View:                                     Top right of the View:
     * (0,0)                                                             (viewSize,0)
     *   +----------------------------------------------------------------+
     *   | <-- GAP --> |=================|              <-- GAP -->       |
     *   +----------------------------------------------------------------+
     *                 ^
     *                 |
     *            Bottom left of the bitmap
     *            (currentTrans, bitmapHeight)
     *
     * Calculation:
     *   1. The total empty space is `viewSize - projectedSize`.
     *   2. Dividing by two, `(viewSize - projectedSize) / 2f`, calculates the size
     *      of the GAP required on each side to place the bitmap evenly inside the view.
     *   3. `(viewSize - projectedSize) / 2f` is therefore the **Target Position**—the ideal
     *      X-coordinate of the bitmap's left edge after validation.
     *   4. The correction (`deltaTrans`) is derived from the relationship:
     *      `currentTrans + deltaTrans = Target Position`
     *   5. Solving for the deltaTrans gives the final formula used in the code:
     *      `deltaTrans = Target Position - currentTrans
     *                  = (viewSize - projectedSize) / 2f - currentTrans`
     * </pre>
     *
     * <pre>
     * <b>Case 2: Gap on the left/top edge</b>
     * The bitmap is larger but panned too far right, creating a gap.
     *
     * Top left of the View:                                        Top right of the View:
     * (0,0)                                                            (viewSize,0)
     *   +----------------------------------------------------------------+
     *   |  <-- GAP -->  |================================================...
     *   +----------------------------------------------------------------+
     *                   ^
     *                   |
     *                 Bottom left of the bit map
     *                (currentTrans, bitmapHeight)
     *
     * Calculation: Return a negative delta (-currentTrans) to move the bitmap's left
     *              edge back to the origin X = 0.
     *
     * Solving for the deltaTrans gives the final formula used in the code:
     *     `deltaTrans = Target Position - currentTrans = 0 - currentTrans`
     * </pre>
     *
     * <pre>
     * <b>Case 3: Gap on the right/bottom edge</b>
     * The Bitmap is larger but panned too far left, creating a gap.
     *
     * Top left of the View:    Top right of the View:
     * (0,0)                     (0,viewSize)
     *   +--------------------------+
     * ...|==================|  <-- GAP -->  |
     *   +--------------------------+
     *                       ^
     *                       |
     *        Bottom right of the bitmap (currentTrans + projectedSize, bitmapHeight)
     *
     * Calculation: Return a positive delta equal to the size of the gap to move the bitmap's right
     * edge to align with the view's right edge. Since the bitmap is wider than the view, this
     * translation safely closes the gap without exposing the left boundary.
     *
     * Solving for the deltaTrans gives the final formula used in the code:
     * `deltaTrans = Target Position - currentTrans = viewSize - (currentTrans + projectedSize)`
     * </pre>
     *
     * <pre>
     * <b>Case 4: No Correction Needed</b>
     * The bitmap is larger and correctly covers the View.
     *
     * Top left of the View:    Top right of the View:
     *      (0,0)                     (0,viewSize)
     *        +--------------------------+
     * ...|===================================================|...
     *        +--------------------------+
     *
     * Calculation: Return 0, since no correction needed.
     * </pre>
     *
     * @param currentTrans The current translation value (e.g., Matrix.MTRANS_X).
     * @param projectedSize The size of the bitmap after scaling (e.g., bitmapWidth * scale).
     * @param viewSize The size of the parent view.
     * @return The delta amount to translate to fix the boundary, or 0 if valid.
     */
    @VisibleForTesting
    static float calculateTranslationCorrection(
            float currentTrans, float projectedSize, int viewSize) {
        // Case 1: The bitmap is smaller than or equal to the view.
        // Theoretically, the caller (validateMatrix) scales the bitmap to fit, so projectedSize
        // should always be >= viewSize. However, we check <= here because floating-point precision
        // errors can sometimes cause the projected size to be slightly smaller than the view. In
        // these cases, we center the bitmap to ensure any resulting gaps are symmetric.
        if (projectedSize <= viewSize) {
            return (viewSize - projectedSize) / 2f - currentTrans;
        }

        // At this point, projectedSize > viewSize.
        // The bitmap is pannable, so the following logic only checks for gaps at the edges.

        // Case 2: A gap exists on the left/top edge of the view.
        // This occurs if the user has panned the bitmap so far right/down or the view is resized
        // that bitmap's top-left corner (currentTrans) is visible within the view (i.e., > 0).
        if (currentTrans > 0) {
            return -currentTrans;
        }

        // Case 3: A gap exists on the right/bottom edge of the view.
        // This occurs if the user has panned so far left/up or the view is resized that the
        // bitmap's right/bottom edge (currentTrans + projectedSize) falls short of the view's edge.
        if (currentTrans + projectedSize < viewSize) {
            return viewSize - (currentTrans + projectedSize);
        }

        // Case 4: No correction is needed.
        // If none of the above conditions were met, it means the bitmap is larger than the view and
        // is panned to a valid position where no gaps are visible inside the view.
        return 0;
    }
}
