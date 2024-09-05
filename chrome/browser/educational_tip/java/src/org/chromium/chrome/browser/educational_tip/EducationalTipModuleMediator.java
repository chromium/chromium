// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the educational tip module. */
public class EducationalTipModuleMediator {
    private static final String FORCE_TAB_GROUP = "force_tab_group";

    private final Context mContext;
    private final @ModuleType int mModuleType;
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final BottomSheetController mBottomSheetController;
    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Runnable mShowTabSwitcherRunnable;
    private final Supplier<ViewGroup> mParentViewSupplier;

    private EducationalTipCardProvider mEducationalTipCardProvider;

    EducationalTipModuleMediator(
            @NonNull Context context,
            @NonNull PropertyModel model,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Runnable showTabSwitcherRunnable,
            @NonNull Supplier<ViewGroup> parentViewSupplier) {
        mContext = context;
        mModuleType = ModuleType.EDUCATIONAL_TIP;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mShowTabSwitcherRunnable = showTabSwitcherRunnable;
        mParentViewSupplier = parentViewSupplier;
    }

    /** Show the educational tip module. */
    void showModule() {
        @EducationalTipCardType int type = getCardType();

        if (type == EducationalTipCardType.TAB_GROUPS) {
            mEducationalTipCardProvider =
                    EducationalTipCardProviderFactory.createInstance(
                            mContext,
                            this::removeModule,
                            mModalDialogManagerSupplier,
                            mShowTabSwitcherRunnable,
                            mParentViewSupplier);

        } else {
            mEducationalTipCardProvider =
                    EducationalTipCardProviderFactory.createInstance(
                            mContext, getCardType(), this::removeModule, mBottomSheetController);
        }

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
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_TAB_GROUP, false)) {
            return EducationalTipCardType.TAB_GROUPS;
        }
        return EducationalTipCardType.DEFAULT_BROWSER_PROMO;
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
    }

    /**
     * Called when the user has viewed the card information to remove the educational tip module
     * from the magic stack.
     */
    private void removeModule() {
        mModuleDelegate.removeModule(mModuleType);
    }
}
