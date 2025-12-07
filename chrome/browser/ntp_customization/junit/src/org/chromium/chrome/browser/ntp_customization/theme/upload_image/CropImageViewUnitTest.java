// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link CropImageView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CropImageViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Define standard dimensions and a delta for float comparisons.
    private static final int PORTRAIT_VIEW_WIDTH = 500;
    private static final int PORTRAIT_VIEW_HEIGHT = 1000;
    private static final int LANDSCAPE_VIEW_WIDTH = 1000;
    private static final int LANDSCAPE_VIEW_HEIGHT = 500;
    private static final float FLOAT_ASSERT_DELTA = 0f;

    private Context mContext;
    private CropImageView mView;
    private ConstraintLayout mParent;

    @Mock private ScaleGestureDetector mScaleDetector;
    @Mock private MotionEvent mMotionEvent1;
    @Mock private MotionEvent mMotionEvent2;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mParent = new ConstraintLayout(mContext);
        mView = new CropImageView(mContext, null);
        ConstraintLayout.LayoutParams params =
                new ConstraintLayout.LayoutParams(
                        ConstraintLayout.LayoutParams.MATCH_PARENT,
                        ConstraintLayout.LayoutParams.MATCH_PARENT);
        mParent.addView(mView, params);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mView.setImageBitmap(bitmap);
    }

    @Test
    public void setImageBitmap_withTallImageInPortrait_isCenterCropped() {
        // Scale = 500 / 400 = 1.25
        // Projected Height = 1600 * 1.25 = 2000
        // Translation Y = (1000 - 2000) / 2 = -500
        assertCenterCropMatrix(
                Bitmap.createBitmap(400, 1600, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT,
                /* expectedScale= */ 1.25f,
                /* expectedTransX= */ 0f,
                /* expectedTransY= */ -500f);
    }

    @Test
    public void setImageBitmap_withWideImageInPortrait_isCenterCropped() {
        // Scale = 1000 / 800 = 1.25
        // Projected Width = 2000 * 1.25 = 2500
        // Translation X = (500 - 2500) / 2 = -1000
        assertCenterCropMatrix(
                Bitmap.createBitmap(2000, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT,
                /* expectedScale= */ 1.25f,
                /* expectedTransX= */ -1000f,
                /* expectedTransY= */ 0f);
    }

    @Test
    public void setImageBitmap_withTallImageInLandscape_isCenterCropped() {
        // Scale = 1000 / 400 = 2.5
        // Projected Height = 1600 * 2.5 = 4000
        // Translation Y = (500 - 4000) / 2 = -1750
        assertCenterCropMatrix(
                Bitmap.createBitmap(400, 1600, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_LANDSCAPE,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT,
                /* expectedScale= */ 2.5f,
                /* expectedTransX= */ 0f,
                /* expectedTransY= */ -1750f);
    }

    @Test
    public void setImageBitmap_withWideImageInLandscape_isCenterCropped() {
        // scaleX = 1000/2000 = 0.5; scaleY = 500/800 = 0.625. Use the larger one.
        // Scale = 0.625
        // Projected Width = 2000 * 0.625 = 1250
        // Translation X = (1000 - 1250) / 2 = -125
        assertCenterCropMatrix(
                Bitmap.createBitmap(2000, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_LANDSCAPE,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT,
                /* expectedScale= */ 0.625f,
                /* expectedTransX= */ -125f,
                /* expectedTransY= */ 0f);
    }

    @Test
    public void setNewBitmap_resetsPreviousCropState() {
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);

        // Simulate a user customizing the crop.
        Matrix customPan = new Matrix(mView.getPortraitMatrix());
        customPan.postTranslate(-100, -100);

        // Set the initialization flag to true, mimicking that this is now a saved, user-customized
        // state
        mView.setPortraitMatrixForTesting(customPan);
        mView.setIsInitializedPortraitForTesting(true);

        setupViewInOrientation(
                Bitmap.createBitmap(600, 600, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);

        Matrix newMatrix = mView.getPortraitMatrix();
        assertMatrixNotEquals(customPan, newMatrix);
    }

    // --- Test Orientation Changes ---

    @Test
    public void rotate_toUninitializedLandscape_preservesFocalPoint() {
        // Start with a default center-crop view in portrait mode.
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);

        // Simulate a user customizing the view (ie: zooming in).
        Matrix customPortraitMatrix = new Matrix();
        customPortraitMatrix.setScale(2.0f, 2.0f);
        mView.setPortraitMatrixForTesting(customPortraitMatrix);
        mView.setIsInitializedPortraitForTesting(true);

        assertFocalPointIsCenteredAfterRotation(
                customPortraitMatrix,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT,
                Configuration.ORIENTATION_LANDSCAPE,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT);
    }

    @Test
    public void rotate_toUninitializedPortrait_preservesFocalPoint() {
        // Start with a default center-crop view in landscape mode.
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_LANDSCAPE,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT);

        // Simulate a user customizing the view (ie: zooming in).
        Matrix customLandscapeMatrix = new Matrix();
        customLandscapeMatrix.setScale(2.0f, 2.0f);
        mView.setLandscapeMatrixForTesting(customLandscapeMatrix);
        mView.setIsInitializedLandscapeForTesting(true);

        assertFocalPointIsCenteredAfterRotation(
                customLandscapeMatrix,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT,
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);
    }

    @Test
    public void rotate_betweenCustomizedOrientations_loadsCorrectSavedState_fromPortrait() {
        setupViewInOrientation(
                Bitmap.createBitmap(1500, 1500, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);

        // Simulate a user panning the image.
        Matrix customPortraitMatrix = new Matrix(mView.getPortraitMatrix());
        customPortraitMatrix.postTranslate(-200f, 0f);
        mView.setPortraitMatrixForTesting(customPortraitMatrix);
        mView.setIsInitializedPortraitForTesting(true);

        // Simulate rotating the device.
        setOrientation(Configuration.ORIENTATION_LANDSCAPE);
        simulateLayoutPass(LANDSCAPE_VIEW_WIDTH, LANDSCAPE_VIEW_HEIGHT);

        // Simulate a different user pan than the one in the portrait mode.
        // Now both orientations have a unique, saved state.
        Matrix customLandscapeMatrix = new Matrix(mView.getLandscapeMatrix());
        customLandscapeMatrix.postTranslate(200f, 0f);
        mView.setLandscapeMatrixForTesting(customLandscapeMatrix);
        mView.setIsInitializedLandscapeForTesting(true);

        // Simulate rotating back to portrait.
        setOrientation(Configuration.ORIENTATION_PORTRAIT);
        simulateLayoutPass(PORTRAIT_VIEW_WIDTH, PORTRAIT_VIEW_HEIGHT);

        // Verify that the view has correctly loaded the original portrait state.
        Matrix finalPortraitMatrix = mView.getPortraitMatrix();
        assertMatrixEquals(customPortraitMatrix, finalPortraitMatrix);

        // Verify that both orientations have a unique, saved state.
        assertMatrixNotEquals(customLandscapeMatrix, finalPortraitMatrix);
    }

    @Test
    public void rotate_betweenCustomizedOrientations_loadsCorrectSavedState_fromLandscape() {
        setupViewInOrientation(
                Bitmap.createBitmap(1500, 1500, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_LANDSCAPE,
                LANDSCAPE_VIEW_WIDTH,
                LANDSCAPE_VIEW_HEIGHT);

        // Simulate a user panning the image.
        Matrix customLandscapeMatrix = new Matrix(mView.getLandscapeMatrix());
        customLandscapeMatrix.postTranslate(200f, 0f);
        mView.setLandscapeMatrixForTesting(customLandscapeMatrix);
        mView.setIsInitializedLandscapeForTesting(true);

        // Simulate rotating the device.
        setOrientation(Configuration.ORIENTATION_PORTRAIT);
        simulateLayoutPass(PORTRAIT_VIEW_WIDTH, PORTRAIT_VIEW_HEIGHT);

        // Simulate a different user pan than the one in the landscape mode.
        // Now both orientations have a unique, saved state.
        Matrix customPortraitMatrix = new Matrix(mView.getPortraitMatrix());
        customPortraitMatrix.postTranslate(-200f, 0f);
        mView.setPortraitMatrixForTesting(customPortraitMatrix);
        mView.setIsInitializedPortraitForTesting(true);

        // Simulate rotating back to landscape.
        setOrientation(Configuration.ORIENTATION_LANDSCAPE);
        simulateLayoutPass(LANDSCAPE_VIEW_WIDTH, LANDSCAPE_VIEW_HEIGHT);

        // Verify that the view has correctly reloaded the original landscape state.
        Matrix finalLandscapeMatrix = mView.getLandscapeMatrix();
        assertMatrixEquals(customLandscapeMatrix, finalLandscapeMatrix);

        // Verify that both orientations have a unique, saved state.
        assertMatrixNotEquals(customPortraitMatrix, finalLandscapeMatrix);
    }

    // --- Behavior Group 3: API Contracts ---

    @Test
    public void getPortraitMatrix_returnsDefensiveCopy() {
        assertMatrixGetterReturnsDefensiveCopy(Configuration.ORIENTATION_PORTRAIT);
    }

    @Test
    public void getLandscapeMatrix_returnsDefensiveCopy() {
        assertMatrixGetterReturnsDefensiveCopy(Configuration.ORIENTATION_LANDSCAPE);
    }

    @Test
    public void getMatrix_forUninitializedLandscape_isCalculatedOnTheFly() {
        // Verifies that Even if an orientation has never been physically displayed, calling its
        // getter method (e.g., getLandscapeMatrix()) will not fail.
        // Instead, it will calculate and return a valid, best-effort matrix on the fly.
        assertMatrixIsCalculatedOnTheFlyForUninitializedOrientation(
                Configuration.ORIENTATION_PORTRAIT, Configuration.ORIENTATION_LANDSCAPE);
    }

    @Test
    public void getMatrix_forUninitializedPortrait_isCalculatedOnTheFly() {
        assertMatrixIsCalculatedOnTheFlyForUninitializedOrientation(
                Configuration.ORIENTATION_LANDSCAPE, Configuration.ORIENTATION_PORTRAIT);
    }

    // --- Helper Methods ---

    /**
     * Sets up the CropImageView with a given bitmap and simulates a layout pass for the specified
     * orientation and dimensions.
     *
     * @param bitmap The bitmap to display.
     * @param orientation The orientation to simulate (e.g., Configuration.ORIENTATION_PORTRAIT).
     * @param width The width for the layout pass.
     * @param height The height for the layout pass.
     */
    private void setupViewInOrientation(Bitmap bitmap, int orientation, int width, int height) {
        setOrientation(orientation);
        mView.setImageBitmap(bitmap);
        simulateLayoutPass(width, height);
    }

    /**
     * A custom assertion that consolidates the logic for verifying the initial center-crop matrix.
     */
    private void assertCenterCropMatrix(
            Bitmap bitmap,
            int orientation,
            int viewWidth,
            int viewHeight,
            float expectedScale,
            float expectedTransX,
            float expectedTransY) {
        setupViewInOrientation(bitmap, orientation, viewWidth, viewHeight);

        Matrix matrix =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? mView.getPortraitMatrix()
                        : mView.getLandscapeMatrix();
        float[] values = getMatrixValues(matrix);

        assertEquals(
                "Scale is incorrect", expectedScale, values[Matrix.MSCALE_X], FLOAT_ASSERT_DELTA);
        assertEquals(
                "Horizontal translation is incorrect",
                expectedTransX,
                values[Matrix.MTRANS_X],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Vertical translation is incorrect",
                expectedTransY,
                values[Matrix.MTRANS_Y],
                FLOAT_ASSERT_DELTA);
    }

    /** A custom assertion that verifies the core focal point preservation logic. */
    private void assertFocalPointIsCenteredAfterRotation(
            Matrix sourceMatrix,
            int fromWidth,
            int fromHeight,
            int toOrientation,
            int toWidth,
            int toHeight) {
        // Calculate the bitmap pixel corresponding to the center of the portrait view
        float[] focalPoint = {fromWidth / 2f, fromHeight / 2f};
        Matrix inverse = new Matrix();
        sourceMatrix.invert(inverse);
        inverse.mapPoints(focalPoint);

        // Simulate rotating the device to landscape for the first time
        setOrientation(toOrientation);
        simulateLayoutPass(toWidth, toHeight);

        // Get the newly calculated landscape matrix and use it to project our original bitmap focal
        // point back onto the screen.
        Matrix newMatrix =
                (toOrientation == Configuration.ORIENTATION_PORTRAIT)
                        ? mView.getPortraitMatrix()
                        : mView.getLandscapeMatrix();
        float[] newScreenPoint = {focalPoint[0], focalPoint[1]};
        newMatrix.mapPoints(newScreenPoint);

        // Verifies if the original focal point is now located at the center of the new landscape
        // screen.
        assertEquals(
                "Focal point X should be centered",
                toWidth / 2f,
                newScreenPoint[0],
                FLOAT_ASSERT_DELTA);
        assertEquals(
                "Focal point Y should be centered",
                toHeight / 2f,
                newScreenPoint[1],
                FLOAT_ASSERT_DELTA);
    }

    /** A custom assertion that verifies the matrix getter returns a defensive copy. */
    private void assertMatrixGetterReturnsDefensiveCopy(int orientation) {
        int width =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? PORTRAIT_VIEW_WIDTH
                        : LANDSCAPE_VIEW_WIDTH;
        int height =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? PORTRAIT_VIEW_HEIGHT
                        : LANDSCAPE_VIEW_HEIGHT;
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888), orientation, width, height);

        Matrix matrix1 =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? mView.getPortraitMatrix()
                        : mView.getLandscapeMatrix();
        Matrix matrixBeforeModification =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? mView.getPortraitMatrix()
                        : mView.getLandscapeMatrix();
        matrix1.postScale(2.0f, 2.0f);
        Matrix matrixAfterModification =
                (orientation == Configuration.ORIENTATION_PORTRAIT)
                        ? mView.getPortraitMatrix()
                        : mView.getLandscapeMatrix();

        // Verifies that the modified copy should not equal internal state.
        assertMatrixNotEquals(matrix1, matrixAfterModification);
        // Verifies that the internal state should not be affected by modification.
        assertMatrixEquals(matrixBeforeModification, matrixAfterModification);
    }

    /** A custom assertion that verifies the on-the-fly matrix calculation works as intended. */
    private void assertMatrixIsCalculatedOnTheFlyForUninitializedOrientation(
            int setupOrientation, int targetOrientation) {
        int setupWidth =
                (setupOrientation == Configuration.ORIENTATION_PORTRAIT)
                        ? PORTRAIT_VIEW_WIDTH
                        : LANDSCAPE_VIEW_WIDTH;
        int setupHeight =
                (setupOrientation == Configuration.ORIENTATION_PORTRAIT)
                        ? PORTRAIT_VIEW_HEIGHT
                        : LANDSCAPE_VIEW_HEIGHT;
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888),
                setupOrientation,
                setupWidth,
                setupHeight);

        Matrix onTheFlyMatrix;
        Matrix alreadyInitializedMatrix;
        if (targetOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            // At this point, landscape is uninitialized.
            // Calling getLandscapeMatrix() should trigger an on-the-fly calculation.
            onTheFlyMatrix = mView.getLandscapeMatrix();
            alreadyInitializedMatrix = mView.getPortraitMatrix();
        } else {
            // At this point, landscape is uninitialized.
            // Calling getPortraitMatrix() should trigger an on-the-fly calculation.
            onTheFlyMatrix = mView.getPortraitMatrix();
            alreadyInitializedMatrix = mView.getLandscapeMatrix();
        }

        // A freshly calculated matrix should not be the default identity matrix,
        // nor should it be the same as the already-initialized portrait matrix.
        assertMatrixNotEquals(new Matrix(), onTheFlyMatrix);
        assertMatrixNotEquals(alreadyInitializedMatrix, onTheFlyMatrix);
    }

    private void setOrientation(int orientation) {
        String qualifier = (orientation == Configuration.ORIENTATION_LANDSCAPE) ? "land" : "port";
        RuntimeEnvironment.setQualifiers(qualifier);
    }

    /**
     * Manually simulates a measure and layout pass for the parent view.
     *
     * <p>In a Robolectric unit test, the Android layout system does not run asynchronously as it
     * would in a real application. A call to ViewUtils.requestLayout in production code only
     * schedules a layout pass for a future frame. Since that "future frame" never arrives in a
     * synchronous test, we must explicitly trigger the measure and layout process ourselves.
     *
     * <p>This method forces the parent view (and therefore its child, the {@code CropImageView}) to
     * be measured with the given dimensions and then laid out. This synchronously triggers the
     * child's {@code onSizeChanged()} method, which is critical for testing the matrix calculation
     * logic.
     *
     * @param width The exact width to assign to the parent view.
     * @param height The exact height to assign to the parent view.
     */
    private void simulateLayoutPass(int width, int height) {
        mParent.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        mParent.layout(0, 0, width, height);
    }

    private static float[] getMatrixValues(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        return values;
    }

    private static void assertMatrixEquals(Matrix expected, Matrix actual) {
        assertArrayEquals(
                "Matrix values should be equal",
                getMatrixValues(expected),
                getMatrixValues(actual),
                FLOAT_ASSERT_DELTA);
    }

    private static void assertMatrixNotEquals(Matrix unexpected, Matrix actual) {
        float[] unexpectedValues = getMatrixValues(unexpected);
        float[] actualValues = getMatrixValues(actual);
        boolean areEqual = true;
        for (int i = 0; i < 9; i++) {
            if (Math.abs(unexpectedValues[i] - actualValues[i]) > FLOAT_ASSERT_DELTA) {
                areEqual = false;
                break;
            }
        }
        if (areEqual) {
            throw new AssertionError("Matrices were expected to be different, but were the same.");
        }
    }

    @Test
    public void testOnScale() {
        assertFalse(mView.getIsScaled());
        CropImageView.ScaleListener scaleListener = mView.new ScaleListener();
        scaleListener.onScale(mScaleDetector);
        assertTrue(mView.getIsScaled());
    }

    @Test
    public void testOnScroll() {
        assertFalse(mView.getIsScrolled());
        CropImageView.GestureListener gestureListener = mView.new GestureListener();
        gestureListener.onScroll(mMotionEvent1, mMotionEvent2, 0, 0);
        assertTrue(mView.getIsScrolled());
    }

    @Test
    public void testConfigureMatrixForCurrentOrientation() {
        setupViewInOrientation(
                Bitmap.createBitmap(800, 800, Bitmap.Config.ARGB_8888),
                Configuration.ORIENTATION_PORTRAIT,
                PORTRAIT_VIEW_WIDTH,
                PORTRAIT_VIEW_HEIGHT);
        assertFalse(mView.getIsScreenRotated());
        setOrientation(Configuration.ORIENTATION_LANDSCAPE);
        mView.configureMatrixForCurrentOrientation(PORTRAIT_VIEW_WIDTH, PORTRAIT_VIEW_HEIGHT);
        assertTrue(mView.getIsScreenRotated());
    }
}
