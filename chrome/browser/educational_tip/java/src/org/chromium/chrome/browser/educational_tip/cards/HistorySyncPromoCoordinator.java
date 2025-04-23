// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;

/**
 * A coordinator that is responsible for displaying the history sync education tip that is show on
 * the NTP in the magic stack if the user is eligible.
 */
public class HistorySyncPromoCoordinator implements EducationalTipCardProvider {

    private static final String HISTORY_OPT_IN_EDUCATIONAL_TIP_PARAM =
            "history_opt_in_educational_tip_param";

    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;

    public HistorySyncPromoCoordinator(
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate) {
        mActionDelegate = actionDelegate;

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            mActionDelegate.showHistorySyncOptIn();
                            onModuleClickedCallback.run();
                        });
    }

    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.educational_tip_history_sync_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_history_sync_description);
    }

    @Override
    public String getCardButtonText() {
        int buttonStringParam =
                SigninFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsInt(
                                SigninFeatures.HISTORY_OPT_IN_EDUCATIONAL_TIP,
                                HISTORY_OPT_IN_EDUCATIONAL_TIP_PARAM,
                                /* defaultValue= */ 0);

        switch (buttonStringParam) {
            case 0:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_turn_on);
            case 1:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_lets_go);
            case 2:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_continue);
            default:
                throw new IllegalStateException(
                        "Invalid variation state for kHistoryOptInEducationalTip");
        }
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.history_sync_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }
}
