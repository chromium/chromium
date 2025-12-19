// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BITMAP_FOR_PREVIEW;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BOTTOM_MARGIN;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_BITMAP;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_PARAMS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_VISIBILITY;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.TOP_INSETS;

import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
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

        if (propertyKey == BITMAP_FOR_PREVIEW) {
            cropImageView.setImageBitmap(model.get(BITMAP_FOR_PREVIEW));
        } else if (propertyKey == PREVIEW_SAVE_CLICK_LISTENER) {
            saveButton.setOnClickListener(model.get(PREVIEW_SAVE_CLICK_LISTENER));
        } else if (propertyKey == PREVIEW_CANCEL_CLICK_LISTENER) {
            cancelButton.setOnClickListener(model.get(PREVIEW_CANCEL_CLICK_LISTENER));
        } else if (propertyKey == LOGO_BITMAP) {
            layout.setLogo(model.get(LOGO_BITMAP));
        } else if (propertyKey == LOGO_PARAMS) {
            int[] heightTopMargin = model.get(LOGO_PARAMS);
            layout.setLogoViewLayoutParams(heightTopMargin[0], heightTopMargin[1]);
        } else if (propertyKey == LOGO_VISIBILITY) {
            layout.setLogoVisibility(model.get(LOGO_VISIBILITY));
        } else if (propertyKey == TOP_INSETS) {
            layout.setTopInsets(model.get(TOP_INSETS));
        } else if (propertyKey == BOTTOM_MARGIN) {
            layout.setBottomInsets(model.get(BOTTOM_MARGIN));
        }
    }
}
