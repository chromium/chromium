// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.MODULE_TITLE;

import android.content.Context;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProviderFactory;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

/**
 * Coordinator for a generic two-cell educational tip container. It is responsible for fetching and
 * preparing the content for the two cells.
 */
@NullMarked
public class EducationalTipModuleTwoCellCoordinator implements ModuleProvider {
    private final @ModuleType int mModuleType;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    private final CallbackController mCallbackController = new CallbackController();
    private final @Nullable EducationalTipCardProvider mItem1Provider;
    private final @Nullable EducationalTipCardProvider mItem2Provider;

    /**
     * @param moduleType The type of the module to build.
     * @param moduleDelegate The instance of {@link ModuleDelegate}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipModuleTwoCellCoordinator(
            @ModuleType int moduleType,
            ModuleDelegate moduleDelegate,
            EducationTipModuleActionDelegate actionDelegate) {
        mModuleType = moduleType;
        mModuleDelegate = moduleDelegate;

        mModel = new PropertyModel.Builder(EducationalTipModuleTwoCellProperties.ALL_KEYS).build();
        mModel.set(
                MODULE_TITLE,
                actionDelegate.getContext().getString(R.string.educational_tip_module_title));

        Runnable removeModuleCallback = () -> mModuleDelegate.removeModule(getModuleType());
        List<Integer> setupListModuleTypes = SetupListModuleUtils.getRankedModuleTypes();

        assert setupListModuleTypes.size() >= 2 : "Two cell layout requires at least two items";
        @ModuleType int item1ModuleType = setupListModuleTypes.get(0);
        @ModuleType int item2ModuleType = setupListModuleTypes.get(1);

        mItem1Provider =
                EducationalTipCardProviderFactory.createInstance(
                        item1ModuleType,
                        () -> {},
                        mCallbackController,
                        actionDelegate,
                        removeModuleCallback);
        if (mItem1Provider != null) {
            mModel.set(ITEM_1_TITLE, mItem1Provider.getCardTitle());
            mModel.set(ITEM_1_DESCRIPTION, mItem1Provider.getCardDescription());
            mModel.set(ITEM_1_CLICK_HANDLER, mItem1Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(mItem1Provider), mModuleType);
            if (completionState == null) {
                mModel.set(ITEM_1_ICON, mItem1Provider.getCardImage());
            } else {
                mModel.set(ITEM_1_MARK_COMPLETED, completionState.isCompleted);
                mModel.set(ITEM_1_ICON, completionState.iconRes);
            }
        }

        mItem2Provider =
                EducationalTipCardProviderFactory.createInstance(
                        item2ModuleType,
                        () -> {},
                        mCallbackController,
                        actionDelegate,
                        removeModuleCallback);
        if (mItem2Provider != null) {
            mModel.set(ITEM_2_TITLE, mItem2Provider.getCardTitle());
            mModel.set(ITEM_2_DESCRIPTION, mItem2Provider.getCardDescription());
            mModel.set(ITEM_2_CLICK_HANDLER, mItem2Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState2 =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(mItem2Provider), mModuleType);
            if (completionState2 == null) {
                mModel.set(ITEM_2_ICON, mItem2Provider.getCardImage());
            } else {
                mModel.set(ITEM_2_MARK_COMPLETED, completionState2.isCompleted);
                mModel.set(ITEM_2_ICON, completionState2.iconRes);
            }
        }
    }

    // ModuleProvider implementation.
    @Override
    public void showModule() {
        mModuleDelegate.onDataReady(mModuleType, mModel);
    }

    @Override
    public void hideModule() {
        mCallbackController.destroy();
        if (mItem1Provider != null) {
            mItem1Provider.destroy();
        }
        if (mItem2Provider != null) {
            mItem2Provider.destroy();
        }
    }

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return context.getString(R.string.educational_tip_module_context_menu_hide);
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public int getModuleType() {
        return mModuleType;
    }
}
