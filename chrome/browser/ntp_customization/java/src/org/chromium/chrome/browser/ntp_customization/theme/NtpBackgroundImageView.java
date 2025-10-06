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
import android.support.annotation.VisibleForTesting;
import android.widget.ImageView;

import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/**
 * An ImageView that displays the New Tab Page background. This view handles different types of
 * background images and applies the correct scaling and transformation.
 */
@NullMarked
public class NtpBackgroundImageView extends ImageView {
    private final Context mContext;
    private final UiConfig mUiConfig;
    private final DisplayStyleObserver mDisplayStyleObserver;
    private int mBackgroundImageType;
    private @Nullable Bitmap mOriginalBitmap;
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;

    public NtpBackgroundImageView(Context context, UiConfig uiConfig) {
        super(context);
        mContext = context;
        mUiConfig = uiConfig;
        mDisplayStyleObserver = (newDisplayStyle) -> setImageBackgroundWithMatrices();
        mUiConfig.addObserver(mDisplayStyleObserver);
    }

    public void setBackground(
            Bitmap originalBitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundImageType int backgroundType) {
        mOriginalBitmap = originalBitmap;
        mBackgroundImageType = backgroundType;

        switch (backgroundType) {
            case THEME_COLLECTION:
                setScaleType(ImageView.ScaleType.CENTER_CROP);
                setImageBitmap(originalBitmap);
                break;

            case IMAGE_FROM_DISK:
                mBackgroundImageInfo = backgroundImageInfo;
                setScaleType(ImageView.ScaleType.MATRIX);
                setImageBitmap(originalBitmap);
                setImageBackgroundWithMatrices();
                break;

            default:
                assert false : "NtpBackgroundImageType not supported!";
        }
    }

    @VisibleForTesting
    void setImageBackgroundWithMatrices() {
        if (mBackgroundImageType != IMAGE_FROM_DISK
                || mOriginalBitmap == null
                || mBackgroundImageInfo == null) return;

        Matrix matrixToApply =
                getValidatedMatrixForCurrentWindowSize(
                        (Activity) mContext, mBackgroundImageInfo, mOriginalBitmap);

        this.setImageMatrix(matrixToApply);
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

    public void destroy() {
        mUiConfig.removeObserver(mDisplayStyleObserver);
    }
}
