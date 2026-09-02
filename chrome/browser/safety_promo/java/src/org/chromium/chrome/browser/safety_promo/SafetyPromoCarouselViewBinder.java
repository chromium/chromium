// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.SUBTITLE_RES_ID;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.TITLE_RES_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for Safety Promo Carousel. */
@NullMarked
class SafetyPromoCarouselViewBinder {
    public static void bind(
            PropertyModel model, SafetyPromoCarouselView view, PropertyKey propertyKey) {
        if (propertyKey == TITLE_RES_ID) {
            view.setTitleText(model.get(TITLE_RES_ID));
        } else if (propertyKey == SUBTITLE_RES_ID) {
            view.setSubtitleText(model.get(SUBTITLE_RES_ID));
        } else if (propertyKey == ON_CONTINUE_CLICKED) {
            view.setContinueButtonOnClickListener(model.get(ON_CONTINUE_CLICKED));
        }
    }
}
