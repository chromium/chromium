// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BITMAP_FOR_PREVIEW;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BUTTON_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_BITMAP;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_PARAMS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_VISIBILITY;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SEARCH_BOX_HEIGHT;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SEARCH_BOX_TOP_MARGIN;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SEARCH_BOX_WIDTH;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SIDE_AND_BOTTOM_INSETS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.TOP_GUIDELINE_BEGIN;

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
        } else if (propertyKey == TOP_GUIDELINE_BEGIN) {
            layout.setTopGuidelineBegin(model.get(TOP_GUIDELINE_BEGIN));
        } else if (SIDE_AND_BOTTOM_INSETS == propertyKey) {
            layout.setSideAndBottomInsets(model.get(SIDE_AND_BOTTOM_INSETS));
        } else if (propertyKey == SEARCH_BOX_WIDTH) {
            layout.setSearchBoxWidth(model.get(SEARCH_BOX_WIDTH));
        } else if (propertyKey == SEARCH_BOX_HEIGHT) {
            layout.setSearchBoxHeight(model.get(SEARCH_BOX_HEIGHT));
        } else if (propertyKey == SEARCH_BOX_TOP_MARGIN) {
            layout.setSearchBoxTopMargin(model.get(SEARCH_BOX_TOP_MARGIN));
        } else if (propertyKey == BUTTON_BOTTOM_MARGIN) {
            layout.setButtonBottomMargin(model.get(BUTTON_BOTTOM_MARGIN));
        }
    }
}
