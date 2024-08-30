// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the educational tip module. */
public class EducationalTipModuleMediator {
    private final Context mContext;
    private final @ModuleType int mModuleType;
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final EducationalTipCardProvider mEducationalTipCardProvider;

    EducationalTipModuleMediator(
            @NonNull Context context,
            @NonNull PropertyModel model,
            @NonNull ModuleDelegate moduleDelegate) {
        mContext = context;
        mModuleType = ModuleType.EDUCATIONAL_TIP;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mEducationalTipCardProvider =
                EducationalTipCardProviderFactory.createInstance(mContext, getCardType());
    }

    /** Show the educational tip module. */
    void showModule() {
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
    private int getCardType() {
        // TODO(b/355015904): add a logic here to integrate with segmentation or feature engagement.
        return EducationalTipCardType.DEFAULT_BROWSER_PROMO;
    }

    @ModuleType
    int getModuleType() {
        return mModuleType;
    }
}
