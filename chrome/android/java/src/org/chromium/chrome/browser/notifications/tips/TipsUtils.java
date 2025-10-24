// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;

import java.util.ArrayList;
import java.util.List;

/** Static utilities for Tips Notifications. */
@NullMarked
public class TipsUtils {
    /**
     * Assembles a {@link FeatureTipPromoData} object containing required UI and callback
     * information for the respective {@link TipsNotificationsFeatureType}'s promo'.
     *
     * @param context The Android {@link Context}.
     * @param featureType The {@link TipsNotificationsFeatureType} to show a promo for.
     */
    public static FeatureTipPromoData getFeatureTipPromoDataForType(
            Context context, @TipsNotificationsFeatureType int featureType) {
        final @StringRes int positiveButtonTextRes;
        final @StringRes int mainPageTitleRes;
        final @StringRes int mainPageDescriptionRes;
        final @StringRes int detailPageTitleRes;
        final List<String> detailPageSteps = new ArrayList<>();

        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_esb;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_esb;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_esb;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_esb));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_esb));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_esb));
                break;
            case TipsNotificationsFeatureType.QUICK_DELETE:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_quick_delete;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_quick_delete;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_quick_delete;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_quick_delete));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_quick_delete));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_quick_delete));
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text_lens;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_lens;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_lens;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_lens;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_lens));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_lens));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_lens));
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_bottom_omnibox;
                mainPageDescriptionRes =
                        R.string.tips_promo_bottom_sheet_description_bottom_omnibox;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox));
                break;
            default:
                assert false : "Invalid feature type: " + featureType;

                positiveButtonTextRes = Resources.ID_NULL;
                mainPageTitleRes = Resources.ID_NULL;
                mainPageDescriptionRes = Resources.ID_NULL;
                detailPageTitleRes = Resources.ID_NULL;
        }

        return new FeatureTipPromoData(
                context.getString(positiveButtonTextRes),
                context.getString(mainPageTitleRes),
                context.getString(mainPageDescriptionRes),
                context.getString(detailPageTitleRes),
                detailPageSteps);
    }
}
