// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;

/** Represents a safety promo item displayed on the Safety FRE promo screens. */
@NullMarked
public class SafetyPromoItem {
    public static final SafetyPromoItem PASSWORD_MANAGER =
            new SafetyPromoItem(
                    R.drawable.safety_promo_password_manager_illustration,
                    R.string.safety_fre_promo_password_manager_title,
                    R.string.safety_fre_promo_password_manager_subtitle);
    public static final SafetyPromoItem HISTORY_QUICK_DELETE =
            new SafetyPromoItem(
                    R.drawable.safety_promo_history_quick_delete_illustration,
                    R.string.safety_fre_promo_history_quick_delete_title,
                    R.string.safety_fre_promo_history_quick_delete_subtitle);
    public static final SafetyPromoItem ENHANCED_SAFE_BROWSING =
            new SafetyPromoItem(
                    R.drawable.safety_promo_enhanced_safe_browsing_illustration,
                    R.string.safety_fre_promo_enhanced_safe_browsing_title,
                    R.string.safety_fre_promo_enhanced_safe_browsing_subtitle);
    public static final SafetyPromoItem INCOGNITO =
            new SafetyPromoItem(
                    R.drawable.safety_promo_incognito_illustration,
                    R.string.safety_fre_promo_incognito_title,
                    R.string.safety_fre_promo_incognito_subtitle);

    public final @DrawableRes int cardIconResId;
    public final @StringRes int cardTitleResId;
    public final @StringRes int cardSubtitleResId;

    private SafetyPromoItem(
            @DrawableRes int cardIconResId,
            @StringRes int cardTitleResId,
            @StringRes int cardSubtitleResId) {
        this.cardIconResId = cardIconResId;
        this.cardTitleResId = cardTitleResId;
        this.cardSubtitleResId = cardSubtitleResId;
    }
}
