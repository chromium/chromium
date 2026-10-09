// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the tips promo bottom sheet properties to the view. */
@NullMarked
class TipsPromoViewBinder {
    private TipsPromoViewBinder() {}

    /**
     * Binds PropertyKeys to View properties for the tips promo bottom sheet.
     *
     * @param model The PropertyModel for the View.
     * @param view The View to be bound.
     * @param key The key that's being bound.
     */
    static void bind(PropertyModel model, TipsPromoView view, PropertyKey key) {
        if (key == TipsPromoProperties.FEATURE_TIP_PROMO_DATA) {
            view.setPromoData(model.get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA));
        } else if (key == TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER) {
            view.setDetailsButtonClickListener(
                    model.get(TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER));
        } else if (key == TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER) {
            view.setSettingsButtonClickListener(
                    model.get(TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER));
        } else if (key == TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER) {
            view.setBackButtonClickListener(
                    model.get(TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER));
        } else if (key == TipsPromoProperties.CURRENT_SCREEN) {
            view.setDisplayedChild(model.get(TipsPromoProperties.CURRENT_SCREEN));
        } else {
            assert false : "Unhandled property: " + key;
        }
    }
}
