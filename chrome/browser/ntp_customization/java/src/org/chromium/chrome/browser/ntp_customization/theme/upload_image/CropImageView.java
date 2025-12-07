// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.ViewUtils;

/**
 * A custom ImageView that allows for panning and zooming a bitmap, maintaining separate crop
 * states for portrait and landscape orientations.
 *
 * <h3>Design</h3>
 * <ol>
 *   <li><b>Source of Truth</b>: Two matrices, {@code mPortraitMatrix} and {@code mLandscapeMatrix},
 *       store the final, saved state for each orientation. They are the only source of truth.
 *   <li><b>Workspace</b>: The live {@code imageMatrix} (named {@code mCurrentMatrix}) is a
 *       temporary workspace for the currently displayed orientation, updated in real-time by
 *       gestures.
 *   <li><b>Central Controller</b>: The {@code configureMatrixForCurrentOrientation} method is the
 *       single point of control, running on layout and after orientation changes to load the
 *       correct state into the workspace.
 *   <li><b>The Boundary of the image</b>: The image is always constrained to fill the view bounds.
 *        The {@code checkBoundsAndApply()} method enforces this after every user gesture and
 *       programmatic change, preventing any empty space.
 * </ol>
 *
 * <h3>Basic Usage</h3>
 * <pre>
 * 1. Set an image using {@link #setImageBitmap(Bitmap)}.
 * 2. The user pans and zooms the image in the UI.
 * 3. Retrieve the final transformation for an orientation using {@link #getPortraitMatrix()} and
 * {@link #getLandscapeMatrix()}
 */
@NullMarked
public class CropImageView extends AppCompatImageView {
    private final Matrix mPortraitMatrix;
    private final Matrix mLandscapeMatrix;
    private final Matrix mCurrentMatrix;
    private final ScaleGestureDetector mScaleDetector;
    private final GestureDetector mGestureDetector;
    private final float[] mMatrixValues;
    private final int mInitialOrientation;
    private boolean mIsPortraitInitialized;
    private boolean mIsLandscapeInitialized;
    private boolean mIsScaled;
    private boolean mIsScrolled;
    private boolean mIsScreenRotated;
    private @Nullable Bitmap mBitmap;

    private static class Dimensions {
        final int mWidth;
        final int mHeight;

        Dimensions(int w, int h) {
            this.mWidth = w;
            this.mHeight = h;
        }
    }

    public CropImageView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        mPortraitMatrix = new Matrix();
        mLandscapeMatrix = new Matrix();
        mCurrentMatrix = new Matrix();
        // A matrix object contains 9 values.
        mMatrixValues = new float[9];
        mIsPortraitInitialized = false;
        mIsLandscapeInitialized = false;
        mIsScaled = false;
        mIsScrolled = false;
        mIsScreenRotated = false;
        mScaleDetector = new ScaleGestureDetector(context, new ScaleListener());
        mGestureDetector = new GestureDetector(context, new GestureListener());
        mInitialOrientation = getResources().getConfiguration().orientation;

        setScaleType(ScaleType.MATRIX);
    }

    /**
     * Sets the bitmap for the view and resets all crop states. This is the main entry point for
     * using the component. Setting a new bitmap invalidates any previous pan/zoom settings for both
     * orientations, forcing a re-initialization on the next layout pass.
     *
     * @param bm The bitmap to display and crop.
     */
    @Override
    public void setImageBitmap(Bitmap bm) {
        super.setImageBitmap(bm);
        mBitmap = bm;
        mIsPortraitInitialized = false;
        mIsLandscapeInitialized = false;

        // This requestLayout() call ensures that if setImageBitmap() is called *after* the initial
        // layout pass, a new layout pass is triggered. This forces onSizeChanged() to be called
        // again, allowing the matrix initialization to finally run with all correct dimension data.
        ViewUtils.requestLayout(this, "CropImageView.setImageBitmap");
    }

    /**
     * This is called when the view's size changes, which reliably happens on the first layout and
     * after every orientation change.
     */
    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (width > 0 && height > 0) {
            configureMatrixForCurrentOrientation(oldWidth, oldHeight);
        }
    }

    /**
     * Handles all touch input, delegating to scale and gesture detectors. After each gesture, it
     * saves the resulting transformation matrix to {@code mPortraitMatrix} or {@code
     * mLandscapeMatrix} based on the current orientation.
     *
     * @param event The motion event.
     * @return Returns True to indicate the touch event is handled by this view.
     */
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        assertNonNull(mBitmap);

        mScaleDetector.onTouchEvent(event);
        mGestureDetector.onTouchEvent(event);
        saveCurrentMatrixToState();
        return true;
    }

    /**
     * Ensures the matrix for the current orientation is initialized and then applies it to the
     * view.
     *
     * @param oldViewWidth The width of the view before the size change.
     * @param oldViewHeight The height of the view before the size change.
     */
    @VisibleForTesting
    void configureMatrixForCurrentOrientation(int oldViewWidth, int oldViewHeight) {
        assertNonNull(mBitmap);

        if (getWidth() == 0 || getHeight() == 0) {
            return;
        }

        int orientation = getResources().getConfiguration().orientation;
        if (!mIsScreenRotated && orientation != mInitialOrientation) {
            mIsScreenRotated = true;
        }

        // Ensure the correct matrix is initialized
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            if (!mIsPortraitInitialized) {
                calculateMatrixForUninitializedOrientation(
                        mPortraitMatrix,
                        Configuration.ORIENTATION_PORTRAIT,
                        getWidth(),
                        getHeight(),
                        oldViewWidth,
                        oldViewHeight);
                mIsPortraitInitialized = true;
            }
            mCurrentMatrix.set(mPortraitMatrix);
        } else {
            if (!mIsLandscapeInitialized) {
                calculateMatrixForUninitializedOrientation(
                        mLandscapeMatrix,
                        Configuration.ORIENTATION_LANDSCAPE,
                        getWidth(),
                        getHeight(),
                        oldViewWidth,
                        oldViewHeight);
                mIsLandscapeInitialized = true;
            }
            mCurrentMatrix.set(mLandscapeMatrix);
        }

        // Apply the matrix to this view and avoid blank space created by floating point issue.
        checkBoundsAndApply();
    }

    /**
     * Creates a matrix for an uninitialized orientation.
     *
     * @param resultMatrix The matrix to populate with the result.
     * @param targetOrientation The orientation for which to calculate a matrix.
     * @param targetWidth The desired width for the new matrix's view.
     * @param targetHeight The desired height for the new matrix's view.
     * @param sourceWidth The width of the view in the "other" orientation.
     * @param sourceHeight The height of the view in the "other" orientation.
     */
    private void calculateMatrixForUninitializedOrientation(
            Matrix resultMatrix,
            int targetOrientation,
            int targetWidth,
            int targetHeight,
            int sourceWidth,
            int sourceHeight) {
        assertNonNull(mBitmap);

        // Determine the state of the other orientation.
        final boolean isPortraitMode = (targetOrientation == Configuration.ORIENTATION_PORTRAIT);
        final boolean isOtherOrientationInitialized =
                isPortraitMode ? mIsLandscapeInitialized : mIsPortraitInitialized;
        final Matrix otherMatrix = isPortraitMode ? mLandscapeMatrix : mPortraitMatrix;

        // If the other orientation is initialized, use it to preserve the visual center point.
        if (isOtherOrientationInitialized && sourceWidth > 0 && sourceHeight > 0) {
            Matrix calculatedMatrix =
                    CropImageUtils.calculateMatrixFromSharedCenter(
                            otherMatrix,
                            targetWidth,
                            targetHeight,
                            sourceWidth,
                            sourceHeight,
                            mBitmap);
            resultMatrix.set(calculatedMatrix);
        } else {
            // Otherwise, perform a standard center-crop.
            CropImageUtils.calculateInitialCenterCropMatrix(
                    resultMatrix, targetWidth, targetHeight, mBitmap);
        }
    }

    /**
     * Returns a valid matrix for the requested orientation by using the best available data. This
     * method prioritizes returning a saved state, then a shared-center calculation, and finally a
     * default crop.
     *
     * @param targetOrientation The orientation to get the matrix for (e.g., {@link
     *     Configuration#ORIENTATION_PORTRAIT}).
     * @return A new, valid {@link Matrix} instance for the given orientation.
     */
    private Matrix getMatrixForOrientation(int targetOrientation) {
        assertNonNull(mBitmap);

        final boolean isTargetPortrait = (targetOrientation == Configuration.ORIENTATION_PORTRAIT);
        final boolean isTargetInitialized =
                isTargetPortrait ? mIsPortraitInitialized : mIsLandscapeInitialized;
        final Matrix targetMatrix = isTargetPortrait ? mPortraitMatrix : mLandscapeMatrix;

        // Case 1: if the target matrix is already initialized.
        if (isTargetInitialized) {
            return new Matrix(targetMatrix);
        }

        // Case 2: if the target matrix is never initialized.
        Matrix resultMatrix = new Matrix();
        Dimensions dimens = getDimensions(targetOrientation);

        calculateMatrixForUninitializedOrientation(
                resultMatrix,
                targetOrientation,
                dimens.mWidth,
                dimens.mHeight,
                getWidth(),
                getHeight());

        // Before returning the newly calculated matrix, run it through the validator.
        // This cleans up any floating-point errors and guarantees the matrix is correct.
        CropImageUtils.validateMatrix(
                resultMatrix, dimens.mWidth, dimens.mHeight, mBitmap, mMatrixValues);

        return resultMatrix;
    }

    /**
     * Saves the current state of the workspace matrix ({@code mCurrentMatrix}) back into the
     * appropriate "source of truth" matrix ({@code mPortraitMatrix} or {@code mLandscapeMatrix}).
     * This method is called after every user gesture.
     */
    private void saveCurrentMatrixToState() {
        int orientation = getResources().getConfiguration().orientation;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mPortraitMatrix.set(mCurrentMatrix);
        } else {
            mLandscapeMatrix.set(mCurrentMatrix);
        }
    }

    /**
     * This method is called after every transformation to correct the scale and translation,
     * preventing any empty space from appearing around the image. It also prevents the user from
     * zooming out too far.
     */
    private void checkBoundsAndApply() {
        assertNonNull(mBitmap);

        CropImageUtils.validateMatrix(
                mCurrentMatrix, getWidth(), getHeight(), mBitmap, mMatrixValues);
        setImageMatrix(mCurrentMatrix);
    }

    @VisibleForTesting
    public class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            float scaleFactor = detector.getScaleFactor();
            mCurrentMatrix.postScale(
                    scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            checkBoundsAndApply();
            mIsScaled = true;
            return true;
        }
    }

    @VisibleForTesting
    public class GestureListener extends GestureDetector.SimpleOnGestureListener {
        /**
         * Handles the initial ACTION_DOWN event of a gesture.
         *
         * <p>By returning true, we signal to the GestureDetector that this listener is interested
         * in the current gesture and that it should continue to process subsequent motion events
         * (like ACTION_MOVE, which triggers onScroll).
         *
         * @return Always true to consume the event and signal interest in the rest of the gesture.
         */
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }

        @Override
        public boolean onScroll(
                @Nullable MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            mCurrentMatrix.postTranslate(-distanceX, -distanceY);
            checkBoundsAndApply();
            mIsScrolled = true;
            return true;
        }
    }

    /**
     * Returns the final transformation matrix for the Portrait orientation.
     *
     * <p>If the portrait view has been customized by the user, this returns the saved state.
     * Otherwise, it calculates an best-effort matrix that preserves the focal point from the
     * landscape view (if available), or defaults to a standard center-crop.
     *
     * @return A new {@link Matrix} instance containing the transformation for portrait mode.
     */
    public Matrix getPortraitMatrix() {
        return getMatrixForOrientation(Configuration.ORIENTATION_PORTRAIT);
    }

    /**
     * Returns the final transformation matrix for the Landscape orientation.
     *
     * <p>If the landscape view has been customized by the user, this returns the saved state.
     * Otherwise, it calculates an best-effort matrix that preserves the focal point from the
     * portrait view (if available), or defaults to a standard center-crop.
     *
     * @return A new {@link Matrix} instance containing the transformation for landscape mode.
     */
    public Matrix getLandscapeMatrix() {
        return getMatrixForOrientation(Configuration.ORIENTATION_LANDSCAPE);
    }

    /**
     * Returns the dimensions for a target orientation. If the target is the current orientation, it
     * returns current dimensions. Otherwise, it returns swapped dimensions.
     */
    private Dimensions getDimensions(int targetOrientation) {
        int currentOrientation = getResources().getConfiguration().orientation;
        int currentWidth = getWidth();
        int currentHeight = getHeight();

        if (targetOrientation == currentOrientation) {
            return new Dimensions(currentWidth, currentHeight);
        } else {
            return new Dimensions(currentHeight, currentWidth);
        }
    }

    void setPortraitMatrixForTesting(Matrix matrix) {
        mPortraitMatrix.set(matrix);
    }

    void setLandscapeMatrixForTesting(Matrix matrix) {
        mLandscapeMatrix.set(matrix);
    }

    void setIsInitializedPortraitForTesting(boolean isInitialized) {
        mIsPortraitInitialized = isInitialized;
    }

    void setIsInitializedLandscapeForTesting(boolean isInitialized) {
        mIsLandscapeInitialized = isInitialized;
    }

    boolean getIsScaled() {
        return mIsScaled;
    }

    boolean getIsScrolled() {
        return mIsScrolled;
    }

    boolean getIsScreenRotated() {
        return mIsScreenRotated;
    }
}
