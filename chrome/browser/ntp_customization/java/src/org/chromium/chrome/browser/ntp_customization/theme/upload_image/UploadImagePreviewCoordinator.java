// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BITMAP_FOR_PREVIEW;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_KEYS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for managing the Upload Image Preview dialog. */
@NullMarked
public class UploadImagePreviewCoordinator {

    private final PropertyModel mPreviewPropertyModel;
    private CropImageView mCropImageView;

    /**
     * The type of user interactions with the Upload Image Preview dialog.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        PreviewInteractionType.CANCEL,
        PreviewInteractionType.SAVE,
        PreviewInteractionType.PINCH_TO_RESIZE,
        PreviewInteractionType.ROTATE_SCREEN,
        PreviewInteractionType.ROTATE_SCREEN_AND_PINCH_TO_RESIZE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreviewInteractionType {
        int CANCEL = 0;
        int SAVE = 1;
        int PINCH_TO_RESIZE = 2; // Scale and scroll
        int ROTATE_SCREEN = 3;
        int ROTATE_SCREEN_AND_PINCH_TO_RESIZE = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * @param activity The activity context.
     * @param bitmap The bitmap to be previewed.
     */
    public UploadImagePreviewCoordinator(
            Activity activity, Bitmap bitmap, Callback<Boolean> onBottomSheetClickedCallback) {
        mPreviewPropertyModel = new PropertyModel(PREVIEW_KEYS);
        View contentView =
                LayoutInflater.from(activity)
                        .inflate(R.layout.ntp_customization_theme_preview_dialog_layout, null);
        mCropImageView = contentView.findViewById(R.id.preview_image);

        final ChromeDialog dialog =
                new ChromeDialog(
                        activity,
                        R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ false);
        dialog.setContentView(contentView);

        PropertyModelChangeProcessor.create(
                mPreviewPropertyModel, contentView, UploadImagePreviewDialogViewBinder::bind);

        mPreviewPropertyModel.set(NtpThemeProperty.BITMAP_FOR_PREVIEW, bitmap);

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER,
                v -> {
                    onSaveButtonClicked(bitmap, onBottomSheetClickedCallback, dialog);
                    NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                            PreviewInteractionType.SAVE);
                    recordPreviewInteractionsMetric();
                });

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER,
                v -> {
                    onBottomSheetClickedCallback.onResult(false);
                    dialog.dismiss();
                    NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                            PreviewInteractionType.CANCEL);
                    recordPreviewInteractionsMetric();
                });

        int saveButtonMarginBottom =
                activity.getResources()
                        .getDimensionPixelSize(R.dimen.ntp_customization_back_button_margin_start);
        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER,
                (view, insets) -> {
                    int navigationBarHeight =
                            insets.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom;
                    ViewGroup.MarginLayoutParams params =
                            (ViewGroup.MarginLayoutParams) view.getLayoutParams();
                    params.bottomMargin = saveButtonMarginBottom + navigationBarHeight;
                    view.setLayoutParams(params);
                    return insets;
                });

        dialog.show();
        NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewShow();
    }

    /**
     * Records the user interaction metrics for the image preview. When the user rotates the screen
     * and pinches to resize together (the order doesn't matter), only {@link
     * PreviewInteractionType#ROTATE_SCREEN_AND_PINCH_TO_RESIZE} is recorded once. Individual {@link
     * PreviewInteractionType#ROTATE_SCREEN} and {@link PreviewInteractionType#PINCH_TO_RESIZE}
     * metrics are not recorded to avoid double counting.
     */
    private void recordPreviewInteractionsMetric() {
        boolean isScaled = mCropImageView.getIsScaled();
        boolean isScrolled = mCropImageView.getIsScrolled();
        boolean isScreenRotated = mCropImageView.getIsScreenRotated();

        if ((isScaled || isScrolled) && isScreenRotated) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.ROTATE_SCREEN_AND_PINCH_TO_RESIZE);
            return;
        }

        if (isScaled || isScrolled) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.PINCH_TO_RESIZE);
            return;
        }

        if (isScreenRotated) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.ROTATE_SCREEN);
        }
    }

    PropertyModel getPropertyModelForTesting() {
        return mPreviewPropertyModel;
    }

    public void destroy() {
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER, null);
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER, null);
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER, null);
    }

    /** The Binder that connects the PropertyModel to the dialog's views. */
    public static class UploadImagePreviewDialogViewBinder {
        static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
            CropImageView cropImageView = view.findViewById(R.id.preview_image);
            TextView saveButton = view.findViewById(R.id.save_button);
            TextView cancelButton = view.findViewById(R.id.cancel_button);

            if (propertyKey == BITMAP_FOR_PREVIEW) {
                cropImageView.setImageBitmap(model.get(BITMAP_FOR_PREVIEW));
            } else if (propertyKey == PREVIEW_SAVE_CLICK_LISTENER) {
                saveButton.setOnClickListener(model.get(PREVIEW_SAVE_CLICK_LISTENER));
            } else if (propertyKey == PREVIEW_CANCEL_CLICK_LISTENER) {
                cancelButton.setOnClickListener(model.get(PREVIEW_CANCEL_CLICK_LISTENER));
            } else if (propertyKey == PREVIEW_SET_WINDOW_INSETS_LISTENER) {
                ViewCompat.setOnApplyWindowInsetsListener(
                        saveButton, model.get(PREVIEW_SET_WINDOW_INSETS_LISTENER));
            }
        }
    }

    /**
     * Called when the save button is clicked.
     *
     * @param bitmap The selected bitmap.
     * @param onBottomSheetClickedCallback The callback to be notified when a bottom sheet button is
     *     clicked.
     * @param dialog The current preview dialog.
     */
    @VisibleForTesting
    void onSaveButtonClicked(
            Bitmap bitmap, Callback<Boolean> onBottomSheetClickedCallback, ChromeDialog dialog) {
        Matrix portraitMatrix = mCropImageView.getPortraitMatrix();
        Matrix landscapeMatrix = mCropImageView.getLandscapeMatrix();
        NtpCustomizationConfigManager.getInstance()
                .onUploadedImageSelected(
                        bitmap, new BackgroundImageInfo(portraitMatrix, landscapeMatrix));

        onBottomSheetClickedCallback.onResult(true);
        dialog.dismiss();
    }

    void setCropImageViewForTesting(CropImageView view) {
        mCropImageView = view;
    }
}
