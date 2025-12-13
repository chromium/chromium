// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.CropImageUtils.getCurrentWindowDimensions;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.CropImageUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the NTP background image. It handles an ImageView which displays the New Tab Page
 * background, and handles different types of background images and applies the correct scaling and
 * transformation.
 */
@NullMarked
public class NtpBackgroundImageCoordinator {
    private final Context mContext;
    private final UiConfig mUiConfig;
    private final DisplayStyleObserver mDisplayStyleObserver;
    private final PropertyModel mPropertyModel;

    private int mBackgroundImageType;
    private boolean mIsObservingDisplayChange;
    private @Nullable Bitmap mOriginalBitmap;
    // Immutable source of truth loaded from disk. Serves as the reference point for
    // deriving transformations when window dimensions change.
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    // Mutable runtime cache. Stores matrices validated for the current window dimensions
    // to avoid redundant invocation of validateMatrix().
    private @Nullable BackgroundImageInfo mCachedBackgroundImageInfo;

    /**
     * @param context The application context.
     * @param rootView The root view to attach to.
     * @param uiConfig The {@link UiConfig} instance.
     * @param backgroundColor The background color to set on the image view.
     */
    public NtpBackgroundImageCoordinator(
            Context context, ViewGroup rootView, UiConfig uiConfig, @ColorInt int backgroundColor) {
        mContext = context;
        mUiConfig = uiConfig;
        mDisplayStyleObserver = (newDisplayStyle) -> setImageBackgroundWithMatrices();

        FrameLayout backgroundImageLayout =
                (FrameLayout)
                        LayoutInflaterUtils.inflate(
                                mContext, R.layout.ntp_customization_background_image_layout, null);

        mPropertyModel = new PropertyModel(NtpBackgroundImageProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel, backgroundImageLayout, NtpBackgroundImageLayoutViewBinder::bind);

        // Applies an opaque background color to prevent any views underneath from being visible.
        mPropertyModel.set(NtpBackgroundImageProperties.BACKGROUND_COLOR, backgroundColor);
        rootView.addView(backgroundImageLayout);
    }

    /**
     * Sets the background image, utilizing the matrix to control its scaling and positioning.
     *
     * @param originalBitmap The original bitmap of the background image without applying any
     *     matrix.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} instance to apply on the bitmap.
     * @param backgroundType The the background image type.
     */
    public void setBackground(
            Bitmap originalBitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundImageType int backgroundType) {
        mOriginalBitmap = originalBitmap;
        mBackgroundImageType = backgroundType;
        mBackgroundImageInfo = backgroundImageInfo;
        mCachedBackgroundImageInfo = BackgroundImageInfo.getDeepCopy(backgroundImageInfo);
        mPropertyModel.set(
                NtpBackgroundImageProperties.IMAGE_SCALE_TYPE, ImageView.ScaleType.MATRIX);
        mPropertyModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, originalBitmap);
        maybeAddDisplayStyleObserver();
    }

    /** Clears the background image. */
    public void clearBackground() {
        mPropertyModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, null);
        maybeRemoveDisplayStyleObserver();
    }

    /**
     * Performs final cleanup of the background image layout, including the removal of all
     * registered observers.
     */
    public void destroy() {
        maybeRemoveDisplayStyleObserver();
    }

    /**
     * Adds the DisplayStyleObserver if hasn't yet, and set the flag mIsObservingDisplayChange to be
     * true.
     *
     * <p>If the observer is already set, this method manually calls {@link
     * #setImageBackgroundWithMatrices()} to ensure the new background is applied immediately.
     */
    private void maybeAddDisplayStyleObserver() {
        if (mIsObservingDisplayChange) {
            setImageBackgroundWithMatrices();
            return;
        }

        mIsObservingDisplayChange = true;
        mUiConfig.addObserver(mDisplayStyleObserver);
    }

    /**
     * Removes the DisplayStyleObserver if has been added before, and reset the flag
     * mIsObservingDisplayChange.
     */
    private void maybeRemoveDisplayStyleObserver() {
        if (!mIsObservingDisplayChange) return;

        mIsObservingDisplayChange = false;
        mUiConfig.removeObserver(mDisplayStyleObserver);
    }

    private void setImageBackgroundWithMatrices() {
        if ((mBackgroundImageType != IMAGE_FROM_DISK && mBackgroundImageType != THEME_COLLECTION)
                || mOriginalBitmap == null
                || mBackgroundImageInfo == null) return;

        Matrix matrixToApply =
                getValidatedMatrixForCurrentWindowSize(
                        (Activity) mContext, mBackgroundImageInfo, mOriginalBitmap);

        mPropertyModel.set(NtpBackgroundImageProperties.IMAGE_MATRIX, matrixToApply);
    }

    /**
     * Calculates and validates the matrix for the current window dimensions.
     *
     * <p>This method first checks the local cache. If the window size matches the cached size, it
     * returns the cached matrix.
     *
     * <p><b>Drift Prevention:</b> On a cache miss, this method calculates a new matrix based on the
     * source of truth ({@code backgroundImageInfo}), but it does not update that source. The source
     * matrix represents the user's original intent. The resulting matrix contains adjustments
     * strictly for the current window bounds (e.g., preventing black bars).
     *
     * <p>If we overwrote the source of truth with these auto-adjustments, repeated window resizing
     * would introduce cumulative floating-point errors and constraint shifts. This causes "drift,"
     * where the image gradually zooms in or moves away from the user's original selection without
     * any user input.
     *
     * @param activity The current activity used to determine window dimensions.
     * @param backgroundImageInfo The source of truth containing the user's original cropping
     *     intent.
     * @param bitmap The bitmap being displayed.
     * @return A matrix that is validated and adjusted for the current window dimensions.
     */
    private Matrix getValidatedMatrixForCurrentWindowSize(
            Activity activity, BackgroundImageInfo backgroundImageInfo, Bitmap bitmap) {
        assertNonNull(mCachedBackgroundImageInfo);
        Point currentWindowSize = getCurrentWindowDimensions(activity);
        int currentOrientation = activity.getResources().getConfiguration().orientation;

        Point cachedSize = mCachedBackgroundImageInfo.getWindowSize(currentOrientation);
        if (currentWindowSize.equals(cachedSize)) {
            // Cache hit: returns the cached matrix immediately.
            return mCachedBackgroundImageInfo.getMatrix(currentOrientation);
        }

        // Cache miss: uses the source of truth matrix to calculate the matrixToApply
        Matrix sourceMatrix = backgroundImageInfo.getMatrix(currentOrientation);

        Matrix matrixToApply = new Matrix(sourceMatrix);
        float[] matrixValues = new float[9];
        matrixToApply.getValues(matrixValues);
        CropImageUtils.validateMatrix(
                matrixToApply, currentWindowSize.x, currentWindowSize.y, bitmap, matrixValues);

        // Updates the cached BackgroundImageInfo
        updateCachedBackgroundInfo(currentOrientation, currentWindowSize, matrixToApply);

        return matrixToApply;
    }

    private void updateCachedBackgroundInfo(
            int currentOrientation, Point currentWindowSize, Matrix matrixToApply) {
        assertNonNull(mCachedBackgroundImageInfo);
        mCachedBackgroundImageInfo.setWindowSize(currentOrientation, currentWindowSize);
        mCachedBackgroundImageInfo.setMatrix(currentOrientation, matrixToApply);
    }

    public PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }

    public @Nullable BackgroundImageInfo getCachedBackgroundImageInfoForTesting() {
        return mCachedBackgroundImageInfo;
    }
}
