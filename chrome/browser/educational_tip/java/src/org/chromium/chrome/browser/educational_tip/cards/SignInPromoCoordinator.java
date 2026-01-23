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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Coordinator for the sign in promo card. */
@NullMarked
public class SignInPromoCoordinator implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public SignInPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.educational_tip_sign_in_promo_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_sign_in_promo_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_sign_in_promo_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.sign_in_promo_logo;
    }

    @Override
    public void onCardClicked() {
        // TODO(crbug.com/469425754): Launch Sign in flow
        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        if (ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SETUP_LIST_SIGN_IN_PROMO_COMPLETED, false)) {
            return true;
        }

        // Check current sign-in status
        Profile profile = mActionDelegate.getProfileSupplier().get();
        if (profile != null) {
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(profile);
            if (identityManager != null && identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                // User is signed in, mark as complete
                ChromeSharedPreferences.getInstance()
                        .writeBoolean(
                                ChromePreferenceKeys.SETUP_LIST_SIGN_IN_PROMO_COMPLETED, true);
                return true;
            }
        }
        return false;
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
    }
}
