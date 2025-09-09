// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

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

import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for managing the Upload Image Preview dialog. */
@NullMarked
public class UploadImagePreviewCoordinator {

    private final PropertyModel mPreviewPropertyModel;
    private final CropImageView mCropImageView;

    /** A callback for receiving the result of the image crop operation. */
    public interface CropResultCallback {
        void onCropResult(Matrix portraitMatrix, Matrix landscapeMatrix);
    }

    /**
     * @param activity The activity context.
     * @param bitmap The bitmap to be previewed.
     * @param onConfirmCallback The callback to be invoked with crop matrices when the user clicks
     *     "Save".
     * @param onCancelCallback The callback to be invoked when the user clicks "Cancel".
     */
    public UploadImagePreviewCoordinator(
            Activity activity,
            Bitmap bitmap,
            CropResultCallback onConfirmCallback,
            Runnable onCancelCallback) {
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
                    Matrix portraitMatrix = mCropImageView.getPortraitMatrix();
                    Matrix landscapeMatrix = mCropImageView.getLandscapeMatrix();
                    onConfirmCallback.onCropResult(portraitMatrix, landscapeMatrix);
                    dialog.dismiss();
                });

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER,
                v -> {
                    onCancelCallback.run();
                    dialog.dismiss();
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
}
