// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;

/** Coordinator for the address bar placement promo card. */
@NullMarked
public class AddressBarPlacementPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public AddressBarPlacementPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_address_bar_placement_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_address_bar_placement_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_button_go_to_settings);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.address_bar_placement_promo_logo;
    }

    @Override
    public void onCardClicked() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mActionDelegate.getContext(), AddressBarSettingsFragment.class);

        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.address_bar_placement_promo_completed_logo;
    }
}
