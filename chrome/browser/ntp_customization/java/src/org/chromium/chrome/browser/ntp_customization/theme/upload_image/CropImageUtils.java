// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import android.graphics.Bitmap;
import android.graphics.Matrix;

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
        // bitmap's
        // focal point (bitmapFocalX, bitmapFocalY) with the center of the new target view.
        // This ensures the user's point of interest remains centered after the rotation.
        float bitmapWidth = bitmap.getWidth();
        float bitmapHeight = bitmap.getHeight();
        float scaleX = (float) targetViewWidth / bitmapWidth;
        float scaleY = (float) targetViewHeight / bitmapHeight;
        float scale = Math.max(scaleX, scaleY);

        float deltaX = (targetViewWidth / 2f) - (bitmapFocalX * scale);
        float deltaY = (targetViewHeight / 2f) - (bitmapFocalY * scale);

        // Step 4: Apply the final transformation.
        resultMatrix.setScale(scale, scale);
        resultMatrix.postTranslate(deltaX, deltaY);
        return resultMatrix;
    }

    /**
     * Corrects the input matrix in terms of the scale and translation to ensure it perfectly fills
     * the given target dimensions, preventing any boundary gaps. This method contains the pure
     * mathematical logic without any UI side effects.
     *
     * <p>This method is to reinforce two rules:
     *
     * <ol>
     *   <li>The image is always zoomed in at least enough to completely fill the screen.
     *   <li>The image is always panned in a way without any boundary gaps.
     * </ol>
     *
     * @param matrixToValidate The matrix to correct. This matrix will be modified in place.
     * @param targetWidth The width of the viewport the matrix must fill.
     * @param targetHeight The height of the viewport the matrix must fill.
     * @param bitmap The bitmap being displayed.
     * @param matrixValues A pre-allocated float array (length 9) for temporary calculations.
     */
    public static void validateMatrix(
            Matrix matrixToValidate,
            int targetWidth,
            int targetHeight,
            Bitmap bitmap,
            float[] matrixValues) {
        if (targetWidth == 0) return;

        // Step 1. Validate Scale:
        // Get the current zoom scale.
        matrixToValidate.getValues(matrixValues);
        float scale = matrixValues[Matrix.MSCALE_X];

        float bitmapWidth = bitmap.getWidth();
        float bitmapHeight = bitmap.getHeight();

        // Calculate the absolute minimum required zoom.
        float minScale =
                Math.max((float) targetWidth / bitmapWidth, (float) targetHeight / bitmapHeight);

        // If the current scale too small, fix it.
        if (scale < minScale) {
            float scaleCorrection = minScale / scale;
            matrixToValidate.postScale(
                    scaleCorrection, scaleCorrection, targetWidth / 2f, targetHeight / 2f);
        }

        // Step 2. Validate Translation:
        // Get the bitmap's current position and size after possibly scaling.
        matrixToValidate.getValues(matrixValues);
        scale = matrixValues[Matrix.MSCALE_X];

        // Get the current horizontal and vertical translation of the workspace matrix.
        float transX = matrixValues[Matrix.MTRANS_X];
        float transY = matrixValues[Matrix.MTRANS_Y];

        // Calculate the final displayed size of the bitmap after scaling.
        float projectedBitmapWidth = bitmapWidth * scale;
        float projectedBitmapHeight = bitmapHeight * scale;

        // Initialize correction deltas. These will store the distance to move the bitmap.
        float deltaX = 0;
        float deltaY = 0;

        // Horizontal Correction: if the projected bitmap is wider than the target view
        if (projectedBitmapWidth > targetWidth) {
            // Check if the left edge has been panned too far right, creating a gap on the left.
            if (transX > 0) {
                deltaX = -transX;
                // Check if the right edge has been panned too far left, creating a gap on the
                // right.
            } else if (transX + projectedBitmapWidth < targetWidth) {
                deltaX = targetWidth - (transX + projectedBitmapWidth);
            }
        } else {
            // The bitmap is equal to the view. Panning is disallowed.
            // The bitmap must be centered horizontally.
            // Formula: (Required Center Position) - (Current Position) = Correction
            deltaX = (targetWidth - projectedBitmapWidth) / 2f - transX;
        }

        // Vertical Correction (logic is identical to the horizontal correction):
        if (projectedBitmapHeight > targetHeight) {
            if (transY > 0) {
                deltaY = -transY;
            } else if (transY + projectedBitmapHeight < targetHeight) {
                deltaY = targetHeight - (transY + projectedBitmapHeight);
            }
        } else {
            deltaY = (targetHeight - projectedBitmapHeight) / 2f - transY;
        }

        // Step3: apply the calculated transition.
        if (deltaX != 0 || deltaY != 0) {
            matrixToValidate.postTranslate(deltaX, deltaY);
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

        float scaleX = (float) viewWidth / bitmapWidth;
        float scaleY = (float) viewHeight / bitmapHeight;

        // The scale must be large enough to fill the view.
        float scale = Math.max(scaleX, scaleY);

        // Center the image
        float deltaX = (viewWidth - (bitmapWidth * scale)) / 2f;
        float deltaY = (viewHeight - (bitmapHeight * scale)) / 2f;

        matrix.reset();
        matrix.setScale(scale, scale);
        matrix.postTranslate(deltaX, deltaY);
    }
}
