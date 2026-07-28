// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/** Represents a card item displayed on the Safety FRE promo screen. */
@NullMarked
public class SafetyPromoCard {
    public static final SafetyPromoCard PASSWORD_MANAGER =
            new SafetyPromoCard(
                    R.drawable.fre_promo_password_manager_illustration,
                    R.string.safety_fre_promo_password_manager_title,
                    R.string.safety_fre_promo_password_manager_subtitle);
    public static final SafetyPromoCard HISTORY_QUICK_DELETE =
            new SafetyPromoCard(
                    R.drawable.fre_promo_history_quick_delete_illustration,
                    R.string.safety_fre_promo_history_quick_delete_title,
                    R.string.safety_fre_promo_history_quick_delete_subtitle);
    public static final SafetyPromoCard ENHANCED_SAFE_BROWSING =
            new SafetyPromoCard(
                    R.drawable.fre_promo_enhanced_safe_browsing_illustration,
                    R.string.safety_fre_promo_enhanced_safe_browsing_title,
                    R.string.safety_fre_promo_enhanced_safe_browsing_subtitle);
    public static final SafetyPromoCard INCOGNITO =
            new SafetyPromoCard(
                    R.drawable.fre_promo_incognito_illustration,
                    R.string.safety_fre_promo_incognito_title,
                    R.string.safety_fre_promo_incognito_subtitle);

    public final @DrawableRes int iconResId;
    public final @StringRes int titleResId;
    public final @StringRes int subtitleResId;

    private SafetyPromoCard(
            @DrawableRes int iconResId, @StringRes int titleResId, @StringRes int subtitleResId) {
        this.iconResId = iconResId;
        this.titleResId = titleResId;
        this.subtitleResId = subtitleResId;
    }
}
