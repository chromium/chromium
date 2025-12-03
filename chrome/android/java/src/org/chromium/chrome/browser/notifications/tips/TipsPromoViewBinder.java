// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** Binds the tips promo bottom sheet properties to the view. */
@NullMarked
public class TipsPromoViewBinder {
    /**
     * Binds PropertyKeys to View properties for the tips promo bottom sheet.
     *
     * @param model The PropertyModel for the View.
     * @param view The View to be bound.
     * @param key The key that's being bound.
     */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == TipsPromoProperties.FEATURE_TIP_PROMO_DATA) {
            FeatureTipPromoData promoData = model.get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA);

            ButtonCompat mainPagePositiveButton =
                    view.findViewById(R.id.tips_promo_settings_button);
            mainPagePositiveButton.setText(promoData.positiveButtonText);
            ButtonCompat detailPagePositiveButton =
                    view.findViewById(R.id.tips_promo_details_settings_button);
            detailPagePositiveButton.setText(promoData.positiveButtonText);
            TextView mainPageTitleView = view.findViewById(R.id.main_page_title_text);
            mainPageTitleView.setText(promoData.mainPageTitle);
            TextView mainPageDescriptionView = view.findViewById(R.id.main_page_description_text);
            mainPageDescriptionView.setText(promoData.mainPageDescription);
            TextView detailPageTitleView = view.findViewById(R.id.details_page_title_text);
            detailPageTitleView.setText(promoData.detailPageTitle);
        } else if (key == TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER) {
            ButtonCompat detailsButton = view.findViewById(R.id.tips_promo_details_button);
            detailsButton.setOnClickListener(
                    model.get(TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER));
        } else if (key == TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER) {
            ButtonCompat settingsButton = view.findViewById(R.id.tips_promo_settings_button);
            settingsButton.setOnClickListener(
                    model.get(TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER));
            ButtonCompat detailsSettingsButton =
                    view.findViewById(R.id.tips_promo_details_settings_button);
            detailsSettingsButton.setOnClickListener(
                    model.get(TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER));
        } else if (key == TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER) {
            ImageButton backButton = view.findViewById(R.id.details_page_back_button);
            backButton.setOnClickListener(
                    model.get(TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER));
        }
    }
}
