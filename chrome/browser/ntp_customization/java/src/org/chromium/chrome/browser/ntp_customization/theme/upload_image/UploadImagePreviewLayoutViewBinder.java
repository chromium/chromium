// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BITMAP_FOR_PREVIEW;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SET_LOGO_BITMAP;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SET_LOGO_PARAMS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SET_LOGO_VISIBILITY;

import android.view.View;
import android.widget.TextView;

import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder class for the UploadImagePreviewLayout. */
@NullMarked
public class UploadImagePreviewLayoutViewBinder {
    public static void bind(
            PropertyModel model, UploadImagePreviewLayout layout, PropertyKey propertyKey) {
        CropImageView cropImageView = layout.findViewById(R.id.preview_image);
        TextView saveButton = layout.findViewById(R.id.save_button);
        TextView cancelButton = layout.findViewById(R.id.cancel_button);
        View logoView = layout.findViewById(R.id.default_search_engine_logo);

        if (propertyKey == BITMAP_FOR_PREVIEW) {
            cropImageView.setImageBitmap(model.get(BITMAP_FOR_PREVIEW));
        } else if (propertyKey == PREVIEW_SAVE_CLICK_LISTENER) {
            saveButton.setOnClickListener(model.get(PREVIEW_SAVE_CLICK_LISTENER));
        } else if (propertyKey == PREVIEW_CANCEL_CLICK_LISTENER) {
            cancelButton.setOnClickListener(model.get(PREVIEW_CANCEL_CLICK_LISTENER));
        } else if (propertyKey == PREVIEW_SET_WINDOW_INSETS_LISTENER) {
            ViewCompat.setOnApplyWindowInsetsListener(
                    saveButton, model.get(PREVIEW_SET_WINDOW_INSETS_LISTENER));
        } else if (propertyKey == SET_LOGO_BITMAP) {
            layout.setLogo(model.get(SET_LOGO_BITMAP));
        } else if (propertyKey == SET_LOGO_PARAMS) {
            int[] heightTopMargin = model.get(SET_LOGO_PARAMS);
            LogoUtils.setLogoViewLayoutParamsForDoodle(
                    logoView, heightTopMargin[0], heightTopMargin[1]);
        } else if (propertyKey == SET_LOGO_VISIBILITY) {
            layout.setLogoVisibility(model.get(SET_LOGO_VISIBILITY));
        }
    }
}
