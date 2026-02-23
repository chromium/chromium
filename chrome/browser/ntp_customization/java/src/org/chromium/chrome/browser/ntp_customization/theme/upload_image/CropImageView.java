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
import android.graphics.Point;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    private final Matrix mCurrentMatrix;
    // A reusable float array to prevent array allocation during hot loops (onScroll/onScale).
    private final float[] mMatrixValues;
    private final int mInitialOrientation;
    private final BackgroundImageInfo mImageInfo;
    private boolean mIsPortraitInitialized;
    private boolean mIsLandscapeInitialized;
    private boolean mIsScaled;
    private boolean mIsScrolled;
    private boolean mIsScreenRotated;
    private @Nullable Bitmap mBitmap;
    private @Nullable ScaleGestureDetector mScaleDetector;
    private @Nullable GestureDetector mGestureDetector;

    public CropImageView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        mImageInfo = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
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
        mInitialOrientation = getCurrentOrientation();

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
     * after every orientation change. onSizeChanged is the "source of truth" where the window size
     * is updated.
     */
    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (width <= 0 || height <= 0) {
            return;
        }

        // By using the full window dimensions, we solve two issues:
        //
        // 1. Alignment with NTP Validation: The New Tab Page validates the matrix against the
        //    full window size. If we calculated it using different values (ie: width and height
        //    of onSizeChanged), the NTP's validator would "correct" it, causing the user's crop
        //    to visibly "drift".
        //
        // 2. Full Background Coverage: The NTP background is drawn across the entire window,
        //    and system UI (status bar, etc.) is rendered on top of it. This method guarantees
        //    the background is always complete, so no gaps can be revealed as insets change.
        Point windowSize = getCurrentWindowDimension();
        mImageInfo.setWindowSize(getCurrentOrientation(), windowSize);
        configureMatrixForCurrentOrientation(windowSize);
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
        // This view will not handle touch events if the image is missing or the view is currently
        // being destroyed.
        if (mBitmap == null || mScaleDetector == null || mGestureDetector == null) {
            return false;
        }

        mScaleDetector.onTouchEvent(event);
        mGestureDetector.onTouchEvent(event);
        saveCurrentMatrixToState();
        return true;
    }

    /**
     * Ensures the matrix for the current orientation is initialized and then applies it to the
     * view.
     *
     * @param windowSize The actual window dimensions for the current orientation.
     */
    @VisibleForTesting
    void configureMatrixForCurrentOrientation(Point windowSize) {
        assertNonNull(mBitmap);

        if (getWidth() == 0 || getHeight() == 0) {
            return;
        }

        int orientation = getCurrentOrientation();
        if (!mIsScreenRotated && orientation != mInitialOrientation) {
            mIsScreenRotated = true;
        }

        // Lazy Initialization: Only calculates the matrix if the user hasn't visited this
        // orientation for the current bitmap yet
        if (!isOrientationInitialized(orientation)) {
            calculateMatrixForUninitializedOrientation(
                    mImageInfo.getMatrix(orientation), orientation, windowSize);
            setOrientationInitialized(orientation);
        }

        // Loads the saved state (Source of Truth) into the Workspace (Live Matrix)
        mCurrentMatrix.set(mImageInfo.getMatrix(orientation));

        // Apply the matrix to this view and avoid blank space created by floating point issue.
        checkBoundsAndApply(windowSize);
    }

    /**
     * Creates a matrix for an uninitialized orientation.
     *
     * @param resultMatrix The matrix to populate with the result.
     * @param targetOrientation The orientation for which to calculate a matrix.
     * @param targetSize The known dimensions for the target orientation.
     */
    private void calculateMatrixForUninitializedOrientation(
            Matrix resultMatrix, int targetOrientation, Point targetSize) {
        assertNonNull(mBitmap);

        int sourceOrientation = getInverseOrientation(targetOrientation);
        boolean isSourceInitialized = isOrientationInitialized(sourceOrientation);

        // If the user has already adjusted the image in the other orientation, use it to preserve
        // the visual center point.
        if (isSourceInitialized) {
            Matrix sourceMatrix = mImageInfo.getMatrix(sourceOrientation);
            Point sourceSize = getWindowSize(sourceOrientation);

            Matrix calculatedMatrix =
                    CropImageUtils.calculateMatrixFromSharedCenter(
                            sourceMatrix,
                            targetSize.x,
                            targetSize.y,
                            sourceSize.x,
                            sourceSize.y,
                            mBitmap);
            resultMatrix.set(calculatedMatrix);
        } else {
            // Otherwise, perform a standard center-crop.
            CropImageUtils.calculateInitialCenterCropMatrix(
                    resultMatrix, targetSize.x, targetSize.y, mBitmap);
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

        // Case 1: if the target matrix is already initialized.
        if (isOrientationInitialized(targetOrientation)) {
            return new Matrix(mImageInfo.getMatrix(targetOrientation));
        }

        // Case 2: if the target matrix is never initialized.
        Matrix resultMatrix = new Matrix();
        Point targetSize = getWindowSize(targetOrientation);
        calculateMatrixForUninitializedOrientation(resultMatrix, targetOrientation, targetSize);

        // Before returning the newly calculated matrix, run it through the validator.
        // This cleans up any floating-point errors and guarantees the matrix is correct.
        CropImageUtils.validateMatrix(
                resultMatrix, targetSize.x, targetSize.y, mBitmap, mMatrixValues);

        return resultMatrix;
    }

    /**
     * Saves the current state of the workspace matrix ({@code mCurrentMatrix}) back into the
     * appropriate "source of truth" matrix ({@code mPortraitMatrix} or {@code mLandscapeMatrix}).
     * This method is called after every user gesture.
     */
    private void saveCurrentMatrixToState() {
        int orientation = getCurrentOrientation();
        mImageInfo.getMatrix(orientation).set(mCurrentMatrix);
    }

    /**
     * This method is called after every transformation to correct the scale and translation,
     * preventing any empty space from appearing around the image. It also prevents the user from
     * zooming out too far.
     *
     * @param windowSize The actual window dimensions for the current orientation.
     */
    private void checkBoundsAndApply(Point windowSize) {
        assertNonNull(mBitmap);

        CropImageUtils.validateMatrix(
                mCurrentMatrix, windowSize.x, windowSize.y, mBitmap, mMatrixValues);
        setImageMatrix(mCurrentMatrix);
    }

    @VisibleForTesting
    public class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            float scaleFactor = detector.getScaleFactor();
            mCurrentMatrix.postScale(
                    scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            checkBoundsAndApply(getWindowSize(getCurrentOrientation()));
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
            checkBoundsAndApply(getWindowSize(getCurrentOrientation()));
            mIsScrolled = true;
            return true;
        }
    }

    public void destroy() {
        mBitmap = null;
        setImageDrawable(null);
        mScaleDetector = null;
        mGestureDetector = null;
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
    Matrix getPortraitMatrix() {
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
    Matrix getLandscapeMatrix() {
        return getMatrixForOrientation(Configuration.ORIENTATION_LANDSCAPE);
    }

    /**
     * Returns the window dimensions for the specified orientation.
     *
     * <p>If the user has actively viewed the screen in the requested {@code orientation}, the exact
     * dimensions will have been captured. If not, it estimates the screen size by swapping the
     * width and height of the current window size.
     *
     * @param orientation The orientation to retrieve dimensions for.
     * @return A {@link Point} representing the width and height of the window.
     */
    Point getWindowSize(int orientation) {
        Point windowSize = mImageInfo.getWindowSize(orientation);
        if (windowSize != null) {
            return windowSize;
        }

        // If the size isn't stored, it must be the other, unvisited orientation. Therefore, we
        // estimate by swapping the current dimensions. The redundant check for the current
        // orientation has been removed.
        Point currentWindowSize = getCurrentWindowDimension();
        return new Point(currentWindowSize.y, currentWindowSize.x);
    }

    @VisibleForTesting
    Point getCurrentWindowDimension() {
        return CropImageUtils.getCurrentWindowDimensions(getContext());
    }

    private int getCurrentOrientation() {
        return getResources().getConfiguration().orientation;
    }

    private boolean isOrientationInitialized(int orientation) {
        return (orientation == Configuration.ORIENTATION_PORTRAIT)
                ? mIsPortraitInitialized
                : mIsLandscapeInitialized;
    }

    private void setOrientationInitialized(int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mIsPortraitInitialized = true;
        } else {
            mIsLandscapeInitialized = true;
        }
    }

    private int getInverseOrientation(int orientation) {
        return (orientation == Configuration.ORIENTATION_PORTRAIT)
                ? Configuration.ORIENTATION_LANDSCAPE
                : Configuration.ORIENTATION_PORTRAIT;
    }

    /**
     * Returns the window dimensions associated with the Portrait Matrix. Uses real dimensions if
     * observed, otherwise falls back to estimation.
     */
    Point getPortraitWindowSize() {
        return getWindowSize(Configuration.ORIENTATION_PORTRAIT);
    }

    /**
     * Returns the window dimensions associated with the Landscape Matrix. Uses real dimensions if
     * observed, otherwise falls back to estimation.
     */
    Point getLandscapeWindowSize() {
        return getWindowSize(Configuration.ORIENTATION_LANDSCAPE);
    }

    void setPortraitMatrixForTesting(Matrix matrix) {
        mImageInfo.getPortraitMatrix().set(matrix);
    }

    void setLandscapeMatrixForTesting(Matrix matrix) {
        mImageInfo.getLandscapeMatrix().set(matrix);
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
