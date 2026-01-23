// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;

/** Coordinator for the Password Checkup promo card. */
@NullMarked
public class PasswordCheckupPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public PasswordCheckupPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        // TODO(crbug.com/469425754): Confirm and add eligibility check
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_password_checkup_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_password_checkup_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_password_checkup_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.password_checkup_promo_logo;
    }

    @Override
    public void onCardClicked() {
        // TODO(crbug.com/469425754): Open password manager
        // Considered complete if the user clicks on the promo
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SETUP_LIST_PASSWORD_CHECKUP_PROMO_COMPLETED, true);

        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(
                        ChromePreferenceKeys.SETUP_LIST_PASSWORD_CHECKUP_PROMO_COMPLETED, false);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.password_checkup_promo_completed_logo;
    }
}
