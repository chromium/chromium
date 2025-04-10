// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;

/**
 * A coordinator that is responsible for displaying the history sync education tip that is show on
 * the NTP in the magic stack if the user is eligible.
 */
public class HistorySyncPromoCoordinator implements EducationalTipCardProvider {

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
    public @DrawableRes int getCardImage() {
        return R.drawable.history_sync_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }
}
