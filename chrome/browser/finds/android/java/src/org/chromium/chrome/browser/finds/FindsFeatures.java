// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Helper methods covering Finds related feature checks and states. */
@NullMarked
public final class FindsFeatures {
    // Feature Names
    public static final String CHROME_FINDS = "ChromeFinds";

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return FindsFeatureMap.getInstance().mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    // Feature Flags
    public static final MutableFlagWithSafeDefault sChromeFinds =
            newMutableFlagWithSafeDefault(CHROME_FINDS, false);

    // Feature Params
    public static final MutableBooleanParamWithSafeDefault sAlwaysShowOptInPromo =
            sChromeFinds.newBooleanParam("always_show_opt_in_promo", false);

    public static final MutableBooleanParamWithSafeDefault sEnableHistoryPageOptIn =
            sChromeFinds.newBooleanParam("enable_history_page_opt_in", false);

    // LINT.IfChange(OptInPromoParams)
    public static final MutableIntParamWithSafeDefault sMaxOptInPromoInteractionCount =
            sChromeFinds.newIntParam("finds_opt_in_promo_max_interacted_count", 2);

    public static final MutableIntParamWithSafeDefault sOptInPromoCooldownDays =
            sChromeFinds.newIntParam("finds_opt_in_promo_cooldown_in_days", 7);
    // LINT.ThenChange(//chrome/browser/finds/core/finds_features.cc:OptInPromoParams)
}
