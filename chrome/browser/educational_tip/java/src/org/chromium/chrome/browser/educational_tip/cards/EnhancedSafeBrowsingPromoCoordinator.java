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
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

/** Coordinator for the enhanced safe browsing promo card. */
@NullMarked
public class EnhancedSafeBrowsingPromoCoordinator
        implements EducationalTipCardProvider, SetupListCompletable {
    private final Runnable mOnModuleClickedCallback;
    private final EducationTipModuleActionDelegate mActionDelegate;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EnhancedSafeBrowsingPromoCoordinator(
            Runnable onModuleClickedCallback, EducationTipModuleActionDelegate actionDelegate) {
        mOnModuleClickedCallback = onModuleClickedCallback;
        mActionDelegate = actionDelegate;
    }

    // EducationalTipCardProvider implementation.
    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_enhanced_safe_browsing_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_enhanced_safe_browsing_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_button_go_to_settings);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.enhanced_safe_browsing_promo_logo;
    }

    @Override
    public void onCardClicked() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mActionDelegate.getContext(), SafeBrowsingSettingsFragment.class);
        mOnModuleClickedCallback.run();
    }

    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.ENHANCED_SAFE_BROWSING_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
    }
}
