// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.educational_tip.cards;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Coordinator for the sign in promo card. */
@NullMarked
public class SignInPromoCoordinator implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;
    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSignInCoordinator;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public SignInPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            mSignInCoordinator =
                    mActionDelegate.createBottomSheetSigninAndHistorySyncCoordinator(
                            new BottomSheetSigninAndHistorySyncCoordinator.Delegate() {},
                            SigninAccessPoint.SET_UP_LIST);
        }
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
        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            assumeNonNull(mSignInCoordinator)
                    .startSigninFlow(mActionDelegate.createSigninBottomSheetConfig());
        } else {
            mActionDelegate.showSignInLegacy();
        }
        mOnModuleClickedCallback.run();
    }

    @Override
    public void destroy() {
        if (mSignInCoordinator != null) {
            mSignInCoordinator.destroy();
            mSignInCoordinator = null;
        }
    }

    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.SIGN_IN_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
    }
}
