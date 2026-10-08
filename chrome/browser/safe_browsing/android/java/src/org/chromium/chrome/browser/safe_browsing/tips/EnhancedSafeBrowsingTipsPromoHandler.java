// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tips.TipsPromoHandler;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;

import java.util.List;

/**
 * {@link TipsPromoHandler} for the Enhanced Safe Browsing tip. Supplies the promo bottom sheet
 * content and navigates the user to the Safe Browsing settings page when the promo is accepted.
 */
@NullMarked
public class EnhancedSafeBrowsingTipsPromoHandler implements TipsPromoHandler {
    @Override
    public FeatureTipPromoData getPromoData(Context context) {
        // The same title is used for both the main and detail pages.
        String title = context.getString(R.string.tips_promo_bottom_sheet_title_esb);
        return new FeatureTipPromoData(
                context.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                title,
                context.getString(R.string.tips_promo_bottom_sheet_description_esb),
                R.drawable.tips_promo_esb_logo,
                title,
                List.of(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_esb),
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_esb),
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_esb)));
    }

    @Override
    public void onPromoAccepted(Context context) {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        context,
                        SafeBrowsingSettingsFragment.class,
                        SafeBrowsingSettingsFragment.createArguments(
                                SettingsAccessPoint.TIPS_NOTIFICATIONS_PROMO));
    }
}
