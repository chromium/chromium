// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;

/** Coordinator for the Tab group promo card. */
public class TabGroupPromoCoordinator implements EducationalTipCardProvider {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param callbackController The instance of {@link CallbackController}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public TabGroupPromoCoordinator(
            @NonNull Runnable onModuleClickedCallback,
            @NonNull CallbackController callbackController,
            @NonNull EducationTipModuleActionDelegate actionDelegate) {
        mActionDelegate = actionDelegate;

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            mActionDelegate.openTabGroupIphDialog();
                            onModuleClickedCallback.run();
                        });
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.educational_tip_tab_group_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tab_group_description);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.tab_group_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }
}
