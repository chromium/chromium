// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

/** Coordinator for the setup list celebratory promo card. */
@NullMarked
public class SetupListCelebratoryPromoCoordinator implements EducationalTipCardProvider {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public SetupListCelebratoryPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.setup_list_celebratory_promo_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.setup_list_celebratory_promo_description);
    }

    @Override
    public String getCardButtonText() {
        return "";
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.setup_list_celebratory_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnModuleClickedCallback.run();
    }

    @Override
    public void onViewCreated() {
        SetupListModuleUtils.setModuleCompleted(
                ModuleType.SETUP_LIST_CELEBRATORY_PROMO, /* silent= */ true);
    }
}
