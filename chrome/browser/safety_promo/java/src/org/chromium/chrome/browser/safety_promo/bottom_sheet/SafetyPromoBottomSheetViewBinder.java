// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.chromium.chrome.browser.safety_promo.bottom_sheet.SafetyPromoBottomSheetProperties.ON_GOT_IT_CLICKED;
import static org.chromium.chrome.browser.safety_promo.bottom_sheet.SafetyPromoBottomSheetProperties.TITLE_RES_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the Safety Promo "Show me how" bottom sheet. */
@NullMarked
class SafetyPromoBottomSheetViewBinder {
    public static void bind(
            PropertyModel model, SafetyPromoBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == TITLE_RES_ID) {
            view.setTitle(model.get(TITLE_RES_ID));
        } else if (propertyKey == ON_GOT_IT_CLICKED) {
            view.setGotItButtonOnClickListener(model.get(ON_GOT_IT_CLICKED));
        }
    }
}
