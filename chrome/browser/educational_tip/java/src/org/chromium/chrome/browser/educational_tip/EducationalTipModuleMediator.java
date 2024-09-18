// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the educational tip module. */
public class EducationalTipModuleMediator {
    @VisibleForTesting static final String FORCE_TAB_GROUP = "force_tab_group";
    @VisibleForTesting static final String FORCE_TAB_GROUP_SYNC = "force_tab_group_sync";
    @VisibleForTesting static final String FORCE_QUICK_DELETE = "force_quick_delete";
    @VisibleForTesting static final String FORCE_DEFAULT_BROWSER = "force_default_browser";

    private final EducationTipModuleActionDelegate mActionDelegate;
    private final @ModuleType int mModuleType;
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final CallbackController mCallbackController;

    private EducationalTipCardProvider mEducationalTipCardProvider;

    EducationalTipModuleMediator(
            @NonNull PropertyModel model,
            @NonNull ModuleDelegate moduleDelegate,
            EducationTipModuleActionDelegate actionDelegate) {
        mModuleType = ModuleType.EDUCATIONAL_TIP;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mActionDelegate = actionDelegate;

        mCallbackController = new CallbackController();
    }

    /** Show the educational tip module. */
    void showModule() {
        Integer cardType = getCardType();
        if (cardType == null) {
            mModuleDelegate.onDataFetchFailed(mModuleType);
            return;
        }

        mEducationalTipCardProvider =
                EducationalTipCardProviderFactory.createInstance(
                        cardType, this::onCardClicked, mCallbackController, mActionDelegate);

        mModel.set(
                EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING,
                mEducationalTipCardProvider.getCardTitle());
        mModel.set(
                EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING,
                mEducationalTipCardProvider.getCardDescription());
        mModel.set(
                EducationalTipModuleProperties.MODULE_CONTENT_IMAGE,
                mEducationalTipCardProvider.getCardImage());
        mModel.set(
                EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER,
                v -> {
                    mEducationalTipCardProvider.onCardClicked();
                });

        mModuleDelegate.onDataReady(mModuleType, mModel);
    }

    @EducationalTipCardType
    private @Nullable Integer getCardType() {
        // TODO(b/355015904): add a logic here to integrate with segmentation or feature engagement.
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_DEFAULT_BROWSER, false)) {
            return EducationalTipCardType.DEFAULT_BROWSER_PROMO;
        } else if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_TAB_GROUP, false)) {
            return EducationalTipCardType.TAB_GROUPS;
        } else if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_TAB_GROUP_SYNC, false)) {
            return EducationalTipCardType.TAB_GROUP_SYNC;
        } else if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_QUICK_DELETE, false)) {
            return EducationalTipCardType.QUICK_DELETE;
        }
        return null;
    }

    @ModuleType
    int getModuleType() {
        return mModuleType;
    }

    void destroy() {
        if (mEducationalTipCardProvider != null) {
            mEducationalTipCardProvider.destroy();
            mEducationalTipCardProvider = null;
        }
        mCallbackController.destroy();
    }

    /** Called when user clicks the card. */
    private void onCardClicked() {
        // TODO(b/): Records metrics for clicking the card.
    }
}
