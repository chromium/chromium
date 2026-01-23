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

/** Coordinator for the Save Passwords promo card. */
@NullMarked
public class SavePasswordsPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public SavePasswordsPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_save_passwords_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.save_passwords_promo_logo;
    }

    @Override
    public void onCardClicked() {
        // TODO(crbug.com/469425754): Displays a bottom sheet with instructions to save passwords
        // along with an animation

        // Considered complete if the user clicks on the promo
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SETUP_LIST_SAVE_PASSWORDS_PROMO_COMPLETED, true);
        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SETUP_LIST_SAVE_PASSWORDS_PROMO_COMPLETED, false);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
    }
}
