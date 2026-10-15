// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.quick_delete.QuickDeleteControllerProvider;
import org.chromium.chrome.browser.quick_delete.R;
import org.chromium.chrome.browser.tips.TipsPromoCustomizer;
import org.chromium.chrome.browser.tips.TipsPromoHandler;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.function.Supplier;

/**
 * {@link TipsPromoHandler} for the Quick Delete tip. Supplies the promo bottom sheet content and
 * opens the Quick Delete dialog when the promo is accepted.
 */
@NullMarked
public class QuickDeleteTipsPromoHandler implements TipsPromoHandler {
    @Override
    public FeatureTipPromoData getPromoData(Context context) {
        return new FeatureTipPromoData(
                context.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                context.getString(R.string.tips_promo_bottom_sheet_title_quick_delete),
                context.getString(R.string.tips_promo_bottom_sheet_description_quick_delete),
                R.raw.tips_notifications_quick_delete_logo_anim,
                context.getString(R.string.tips_promo_bottom_sheet_title_quick_delete_short),
                List.of(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_quick_delete),
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_quick_delete),
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_quick_delete)));
    }

    @Override
    public void onPromoAccepted(WindowAndroid windowAndroid) {
        Supplier<QuickDeleteController> controllerFactory =
                QuickDeleteControllerProvider.from(windowAndroid);
        // The hosting activity is responsible for attaching a factory to its window; the promo is
        // only shown from windows that support Quick Delete.
        assert controllerFactory != null : "No QuickDeleteController factory attached to window.";
        if (controllerFactory == null) return;
        controllerFactory.get().showDialog();
    }

    @Override
    public void onPromoShown(TipsPromoCustomizer customizer) {
        customizer.setLogoAnimation(R.raw.tips_notifications_quick_delete_logo_anim);
    }
}
