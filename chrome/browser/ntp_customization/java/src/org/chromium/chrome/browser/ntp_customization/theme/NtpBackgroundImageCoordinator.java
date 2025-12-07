// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;

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
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;

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
        mPropertyModel.set(
                NtpBackgroundImageProperties.IMAGE_SCALE_TYPE, ImageView.ScaleType.MATRIX);
        mPropertyModel.set(NtpBackgroundImageProperties.BACKGROUND_IMAGE, originalBitmap);
        maybeAddDisplayStyleObserver();
        setImageBackgroundWithMatrices();
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
     */
    private void maybeAddDisplayStyleObserver() {
        if (mIsObservingDisplayChange) return;

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

    private Matrix getValidatedMatrixForCurrentWindowSize(
            Activity activity, BackgroundImageInfo backgroundImageInfo, Bitmap bitmap) {
        Point windowSize = getCurrentWindowDimensions(activity);
        boolean isLandscape = windowSize.x > windowSize.y;
        Matrix matrixToApply =
                isLandscape
                        ? backgroundImageInfo.landscapeMatrix
                        : backgroundImageInfo.portraitMatrix;

        float[] matrixValues = new float[9];
        matrixToApply.getValues(matrixValues);

        CropImageUtils.validateMatrix(
                matrixToApply, windowSize.x, windowSize.y, bitmap, matrixValues);
        return matrixToApply;
    }

    private Point getCurrentWindowDimensions(Activity activity) {
        WindowMetrics metrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(activity);
        Rect bounds = metrics.getBounds();
        return new Point(bounds.width(), bounds.height());
    }

    public PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }
}
