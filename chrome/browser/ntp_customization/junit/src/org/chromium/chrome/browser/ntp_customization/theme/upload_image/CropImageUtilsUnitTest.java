// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;
import android.graphics.Matrix;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link CropImageUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CropImageUtilsUnitTest {

    private static final float FLOAT_ASSERT_DELTA = 0.001f; // Use a small delta for float compare

    @Test
    public void testCalculateInitialCenterCropMatrix_forWideImage() {
        Bitmap wideBitmap = Bitmap.createBitmap(400, 100, Bitmap.Config.ARGB_8888);
        Matrix resultMatrix = new Matrix();

        CropImageUtils.calculateInitialCenterCropMatrix(resultMatrix, 100, 200, wideBitmap);

        float[] values = getMatrixValues(resultMatrix);
        assertEquals(
                "Scale X should match height ratio",
                2.0f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Scale Y should match height ratio",
                2.0f,
                values[Matrix.MSCALE_Y],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation X should center horizontally",
                -350f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation Y should be zero", 0f, values[Matrix.MTRANS_Y], FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateInitialCenterCropMatrix_forTallImage() {
        Bitmap tallBitmap = Bitmap.createBitmap(50, 400, Bitmap.Config.ARGB_8888);
        Matrix resultMatrix = new Matrix();

        CropImageUtils.calculateInitialCenterCropMatrix(resultMatrix, 100, 200, tallBitmap);

        float[] values = getMatrixValues(resultMatrix);
        assertEquals(
                "Scale X should match width ratio",
                2.0f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Scale Y should match width ratio",
                2.0f,
                values[Matrix.MSCALE_Y],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation X should be zero", 0f, values[Matrix.MTRANS_X], FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation Y should center vertically",
                -300f,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testValidateMatrix_correctsTooSmallScale() {
        Bitmap bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
        Matrix matrixToFix = new Matrix();
        matrixToFix.setScale(0.5f, 0.5f); // This scale is too small.

        CropImageUtils.validateMatrix(matrixToFix, 200, 300, bitmap, new float[9]);

        float[] values = getMatrixValues(matrixToFix);
        assertEquals(
                "Scale should be corrected up to the minimum",
                0.75f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testValidateMatrix_correctsPanPastRightEdge() {
        Bitmap bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
        Matrix matrixToFix = new Matrix();
        matrixToFix.setScale(1.0f, 1.0f);
        // Pan so the image's right edge (at x=400) is at screen coordinate x=150.
        // The view width is 200, so this creates a 50px gap on the right.
        matrixToFix.postTranslate(-250f, 0f);

        CropImageUtils.validateMatrix(matrixToFix, 200, 200, bitmap, new float[9]);

        float[] values = getMatrixValues(matrixToFix);
        // The right edge (400) must align with the view's right edge (200).
        // So, the final translation must be 200 - 400 = -200.
        assertEquals(
                "Translation X should be corrected to align right edges",
                -200f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testValidateMatrix_correctsPanPastTopEdge() {
        Bitmap bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
        Matrix matrixToFix = new Matrix();
        matrixToFix.setScale(2.0f, 2.0f);
        // Pan down by 100px, creating a gap at the top.
        matrixToFix.postTranslate(0f, 100f);

        CropImageUtils.validateMatrix(matrixToFix, 200, 200, bitmap, new float[9]);

        float[] values = getMatrixValues(matrixToFix);
        // The top edge must be at y=0, so the translation must be corrected back to 0.
        assertEquals(
                "Translation Y should be corrected to align top edges",
                0f,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateMinScaleToFill() {
        // For a wide bitmap, scale should be determined by height ratio
        assertEquals(
                "Wide image in tall view",
                2.0f, // 200 / 100
                CropImageUtils.calculateMinScaleToFill(100, 200, 400f, 100f),
                FLOAT_ASSERT_DELTA);

        // For a tall bitmap, scale should be determined by width ratio
        assertEquals(
                "Tall image in wide view",
                2.0f, // 100 / 50
                CropImageUtils.calculateMinScaleToFill(100, 200, 50f, 400f),
                FLOAT_ASSERT_DELTA);

        // For a bitmap with same aspect ratio, scales are equal
        assertEquals(
                "Image with same aspect ratio",
                0.5f, // 100 / 200
                CropImageUtils.calculateMinScaleToFill(100, 200, 200f, 400f),
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateFocalScale() {
        // Focal point is near an edge, requiring larger scale to keep it centered
        assertEquals(
                "Focal point near edge",
                2.0f, // (200 / 2) / 50
                CropImageUtils.calculateFocalScale(200, 400f, 50f),
                FLOAT_ASSERT_DELTA);

        // Focal point is in the center, requiring minimum scale
        assertEquals(
                "Focal point centered",
                0.5f, // (200 / 2) / 200
                CropImageUtils.calculateFocalScale(200, 400f, 200f),
                FLOAT_ASSERT_DELTA);

        // Focal point is exactly on the left edge, should return 0 to avoid div-by-zero
        assertEquals(
                "Focal point on left edge",
                0f,
                CropImageUtils.calculateFocalScale(200, 400f, 0f),
                FLOAT_ASSERT_DELTA);

        // Focal point is exactly on the right edge, should return 0 to avoid div-by-zero
        assertEquals(
                "Focal point on right edge",
                0f,
                CropImageUtils.calculateFocalScale(200, 400f, 400f),
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateTranslationCorrection() {
        // Case 1: Image is smaller, should be centered.
        // Target position is (100 - 80)/2 = 10. Current translation is 0. Delta translation = 10.
        assertEquals(
                "Smaller image at origin should be moved to center",
                10f,
                CropImageUtils.calculateTranslationCorrection(0f, 80f, 100),
                FLOAT_ASSERT_DELTA);
        // Image is already centered. Delta translation = 0.
        assertEquals(
                "Smaller, centered image needs no correction",
                0f,
                CropImageUtils.calculateTranslationCorrection(10f, 80f, 100),
                FLOAT_ASSERT_DELTA);

        // Case 2: Image is same size, should be at origin.
        // Current is 20. Delta translation = -20.
        assertEquals(
                "Same-size image shifted right should be moved to origin",
                -20f,
                CropImageUtils.calculateTranslationCorrection(20f, 100f, 100),
                FLOAT_ASSERT_DELTA);

        // Case 3: Image is larger, gap on the left.
        // Current is 50. Delta translation = -50.
        assertEquals(
                "Larger image with gap on left needs correction",
                -40f,
                CropImageUtils.calculateTranslationCorrection(40f, 200f, 100),
                FLOAT_ASSERT_DELTA);

        // Case 4: Image is larger, gap on the right.
        // Current is -120. Right edge is at -120+200=80. Gap is 100-80=20. Delta translation = 20.
        assertEquals(
                "Larger image with gap on right needs correction",
                20f,
                CropImageUtils.calculateTranslationCorrection(-120f, 200f, 100),
                FLOAT_ASSERT_DELTA);

        // Case 5: Image is larger and in a valid panned position.
        assertEquals(
                "Larger image at left edge needs no correction",
                0f,
                CropImageUtils.calculateTranslationCorrection(0f, 200f, 100),
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Larger image at right edge needs no correction",
                0f,
                CropImageUtils.calculateTranslationCorrection(-100f, 200f, 100),
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Larger image panned validly needs no correction",
                0f,
                CropImageUtils.calculateTranslationCorrection(-50f, 200f, 100),
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateMatrixFromSharedCenter_orientationChanged() {
        Bitmap bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
        Matrix sourceMatrix = new Matrix();
        sourceMatrix.setScale(2.0f, 2.0f); // User zoomed 2x on top-left of a 100x200 view

        // The center of the 100x200 source view is (50, 100). With a 2x scale, this corresponds to
        // the focal point (25, 50) on the original bitmap. This test verifies that after resizing
        // the view to 200x100, the new matrix keeps this focal point centered.
        Matrix resultMatrix =
                CropImageUtils.calculateMatrixFromSharedCenter(
                        sourceMatrix,
                        /* targetViewWidth= */ 200,
                        /* targetViewHeight= */ 100,
                        /* sourceViewWidth= */ 100,
                        /* sourceViewHeight= */ 200,
                        bitmap);

        float[] values = getMatrixValues(resultMatrix);

        // Scale Logic:
        // - Standard Scale (to cover view): max(200/400, 100/400) = 0.5
        // - FocalScale X:
        //     Dist to edge = min(25, 400-25) = 25.
        //     Scale needed to cover half target width (100): 100 / 25 = 4.0
        // - FocalScale Y:
        //     Distance to edge = min(50, 400-50) = 50.
        //     Scale needed to cover half target height (50): 50 / 50 = 1.0
        // Final Scale = max(0.5, 4.0, 1.0) = 4.0
        assertEquals(
                "Scale should be the minimum required to keep the focal point centered",
                4.0f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);

        // Translation Logic:
        // deltaX = 200 / 2 - 25 * 4 = 100 - 100 = 0
        assertEquals(
                "Translation X should center the focal point",
                0f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);

        // deltaY = 100 / 2 - 50 * 4 = 50 - 200 = -150
        assertEquals(
                "Translation Y should center the focal point",
                -150f,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateMatrixFromSharedCenter_sourceSmallerThanTarget() {
        Bitmap bitmap = Bitmap.createBitmap(800, 600, Bitmap.Config.ARGB_8888);
        Matrix sourceMatrix = new Matrix();
        sourceMatrix.setScale(2.0f, 2.0f);
        sourceMatrix.postTranslate(50f, 50f); // User shifted image in a 200x400 view

        // The center of the 200x400 source view is (100, 200).
        // Inverse calculation for focal point:
        // x = (100 - 50) / 2 = 25
        // y = (200 - 50) / 2 = 75
        // Focal point on bitmap is (25, 75).
        Matrix resultMatrix =
                CropImageUtils.calculateMatrixFromSharedCenter(
                        sourceMatrix,
                        /* targetViewWidth= */ 1200,
                        /* targetViewHeight= */ 600,
                        /* sourceViewWidth= */ 200,
                        /* sourceViewHeight= */ 400,
                        bitmap);

        float[] values = getMatrixValues(resultMatrix);

        // Scale Logic:
        // - Standard Scale: max(1200/800, 600/600) = 1.5
        // - FocalScale X:
        //     Dist to edge = min(25, 800-25) = 25.
        //     Scale needed to cover half target width (600): 600 / 25 = 24.0
        // - FocalScale Y:
        //     Dist to edge = min(75, 600-75) = 75.
        //     Scale needed to cover half target height (300): 300 / 75 = 4.0
        // Final Scale = max(1.5, 24.0, 4.0) = 24.0
        assertEquals(
                "Scale should be dominated by the tight X-axis focal constraint",
                24.0f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);

        // Translation Logic:
        // deltaX = 1200 / 2 - 25 * 24 = 600 - 600 = 0
        assertEquals(
                "Translation X should center the focal point",
                0f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);

        // deltaY = 600 / 2 - 75 * 24 = 300 - 1800 = -1500
        assertEquals(
                "Translation Y should center the focal point",
                -1500f,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    @Test
    public void testCalculateMatrixFromSharedCenter_sourceLargerThanTarget() {
        Bitmap bitmap = Bitmap.createBitmap(1000, 1000, Bitmap.Config.ARGB_8888);
        Matrix sourceMatrix = new Matrix();
        sourceMatrix.setScale(0.5f, 0.5f);
        sourceMatrix.postTranslate(100f, 100f); // Zoomed out and shifted in an 800x800 view

        // The center of the 800x800 source view is (400, 400).
        // Inverse calculation for focal point:
        // x = (400 - 100) / 0.5 = 600
        // y = (400 - 100) / 0.5 = 600
        // Focal point on bitmap is (600, 600).
        Matrix resultMatrix =
                CropImageUtils.calculateMatrixFromSharedCenter(
                        sourceMatrix,
                        /* targetViewWidth= */ 400,
                        /* targetViewHeight= */ 200,
                        /* sourceViewWidth= */ 800,
                        /* sourceViewHeight= */ 800,
                        bitmap);

        float[] values = getMatrixValues(resultMatrix);

        // Scale Logic:
        // - Standard Scale: max(400/1000, 200/1000) = 0.4
        // - FocalScale X:
        //     Dist to edge = min(600, 1000-600) = 400.
        //     Scale needed to cover half target width (200): 200 / 400 = 0.5
        // - FocalScale Y:
        //     Dist to edge = min(600, 1000-600) = 400.
        //     Scale needed to cover half target height (100): 100 / 400 = 0.25
        // Final Scale = max(0.4, 0.5, 0.25) = 0.5
        assertEquals(
                "Scale should be defined by the X-axis focal constraint",
                0.5f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);

        // Translation Logic:
        // deltaX = 400 / 2 - 600 * 0.5 = 200 - 300 = -100
        assertEquals(
                "Translation X should center the focal point",
                -100f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);

        // deltaY = 200 / 2 - 600 * 0.5 = 100 - 300 = -200
        assertEquals(
                "Translation Y should center the focal point",
                -200f,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    // --- Helper Methods ---
    private static float[] getMatrixValues(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        return values;
    }
}
