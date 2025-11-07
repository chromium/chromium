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

    private static final float FLOAT_ASSERT_DELTA = 0f;

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
    public void testCalculateMatrixFromSharedCenter() {
        Bitmap bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
        Matrix sourceMatrix = new Matrix();
        sourceMatrix.setScale(2.0f, 2.0f); // User zoomed 2x on top-left of a 100x200 view

        Matrix resultMatrix =
                CropImageUtils.calculateMatrixFromSharedCenter(
                        sourceMatrix, 200, 100, 100, 200, bitmap);

        float[] values = getMatrixValues(resultMatrix);
        assertEquals(
                "Scale should be the minimum for the target view",
                0.5f,
                values[Matrix.MSCALE_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation X should center the focal point",
                87.5f,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Translation Y should center the focal point",
                25f,
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
