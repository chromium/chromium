// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lens.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tips.TipsPromoCustomizer;
import org.chromium.chrome.browser.tips.TipsPromoHandler;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * {@link TipsPromoHandler} for the Google Lens tip. Supplies the promo bottom sheet content and
 * launches Google Lens when the promo is accepted.
 */
@NullMarked
public class GoogleLensTipsPromoHandler implements TipsPromoHandler {
    @Override
    public FeatureTipPromoData getPromoData(Context context) {
        // The same title is used for both the main and detail pages.
        String title = context.getString(R.string.tips_promo_bottom_sheet_title_lens);
        return new FeatureTipPromoData(
                context.getString(R.string.tips_promo_bottom_sheet_positive_button_text_lens),
                title,
                context.getString(R.string.tips_promo_bottom_sheet_description_lens),
                R.drawable.tips_promo_lens_logo,
                title,
                List.of(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_lens),
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_lens),
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_lens)));
    }

    @Override
    public void onPromoAccepted(WindowAndroid windowAndroid) {
        LensMetrics.recordClicked(LensEntryPoint.TIPS_NOTIFICATIONS);
        LensController.getInstance()
                .startLens(
                        windowAndroid,
                        new LensIntentParams.Builder(
                                        LensEntryPoint.TIPS_NOTIFICATIONS,
                                        isIncognito(windowAndroid))
                                .build());
    }

    @Override
    public void onPromoShown(TipsPromoCustomizer customizer) {
        LensMetrics.recordShown(LensEntryPoint.TIPS_NOTIFICATIONS, /* isShown= */ true);
    }

    /**
     * Returns whether the window's current tab model is Incognito branded, defaulting to false when
     * no {@link TabModelSelector} is attached (matching the activity's empty tab model fallback).
     */
    private static boolean isIncognito(WindowAndroid windowAndroid) {
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(windowAndroid);
        return selector != null && selector.isIncognitoBrandedModelSelected();
    }
}
