// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;

/** Coordinator for the Tips Notifications promo card. */
@NullMarked
public class TipsNotificationsPromoCoordinator implements EducationalTipCardProvider {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param callbackController The instance of {@link CallbackController}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public TipsNotificationsPromoCoordinator(
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate) {
        mActionDelegate = actionDelegate;

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            mActionDelegate.showTipsNotificationsChannelSettings();
                            onModuleClickedCallback.run();
                        });
    }

    // EducationalTipCardProvider implementation.

    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.tips_notifications_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }
}
