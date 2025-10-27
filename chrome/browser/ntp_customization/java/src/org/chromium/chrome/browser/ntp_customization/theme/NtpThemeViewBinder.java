// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.view.View;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Handles the binding of display and interaction for the modules in the theme bottom sheet. */
@NullMarked
public class NtpThemeViewBinder {
    @VisibleForTesting
    public static void bindThemeBottomSheet(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LEARN_MORE_BUTTON_CLICK_LISTENER) {
            ImageView learnMoreButton =
                    view.findViewById(
                            org.chromium.chrome.browser.ntp_customization.R.id.learn_more_button);
            learnMoreButton.setOnClickListener(model.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == IS_SECTION_TRAILING_ICON_VISIBLE) {
            ((NtpThemeBottomSheetView) view)
                    .setSectionTrailingIconVisibility(
                            model.get(IS_SECTION_TRAILING_ICON_VISIBLE).first,
                            model.get(IS_SECTION_TRAILING_ICON_VISIBLE).second);
        } else if (propertyKey == SECTION_ON_CLICK_LISTENER) {
            ((NtpThemeBottomSheetView) view)
                    .setSectionOnClickListener(
                            model.get(SECTION_ON_CLICK_LISTENER).first,
                            model.get(SECTION_ON_CLICK_LISTENER).second);
        } else if (propertyKey == LEADING_ICON_FOR_THEME_COLLECTIONS) {
            ((NtpThemeBottomSheetView) view)
                    .setLeadingIconForThemeCollections(
                            model.get(LEADING_ICON_FOR_THEME_COLLECTIONS));
        }
    }
}
