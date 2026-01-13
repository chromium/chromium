// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_ICON;
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
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Coordinator for a generic two-cell educational tip container. It is responsible for fetching and
 * preparing the content for the two cells.
 */
@NullMarked
public class EducationalTipModuleTwoCellCoordinator implements ModuleProvider {
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    private final CallbackController mCallbackController = new CallbackController();
    private final @Nullable EducationalTipCardProvider mItem1Provider;
    private final @Nullable EducationalTipCardProvider mItem2Provider;

    /**
     * @param moduleDelegate The instance of {@link ModuleDelegate}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipModuleTwoCellCoordinator(
            ModuleDelegate moduleDelegate, EducationTipModuleActionDelegate actionDelegate) {
        mModuleDelegate = moduleDelegate;

        mModel = new PropertyModel.Builder(EducationalTipModuleTwoCellProperties.ALL_KEYS).build();
        mModel.set(
                MODULE_TITLE,
                actionDelegate.getContext().getString(R.string.educational_tip_module_title));

        Runnable removeModuleCallback = () -> mModuleDelegate.removeModule(getModuleType());
        List<Integer> setupListModuleTypes = SetupListModuleUtils.getRankedModuleTypes();

        assert setupListModuleTypes.size() >= 2 : "Two cell layout requires at least two items";

        mItem1Provider =
                EducationalTipCardProviderFactory.createInstance(
                        setupListModuleTypes.get(0),
                        () -> {},
                        mCallbackController,
                        actionDelegate,
                        removeModuleCallback);
        if (mItem1Provider != null) {
            mModel.set(ITEM_1_TITLE, mItem1Provider.getCardTitle());
            mModel.set(ITEM_1_DESCRIPTION, mItem1Provider.getCardDescription());
            mModel.set(ITEM_1_ICON, mItem1Provider.getCardImage());
            mModel.set(ITEM_1_CLICK_HANDLER, mItem1Provider::onCardClicked);
        }

        mItem2Provider =
                EducationalTipCardProviderFactory.createInstance(
                        setupListModuleTypes.get(1),
                        () -> {},
                        mCallbackController,
                        actionDelegate,
                        removeModuleCallback);
        if (mItem2Provider != null) {
            mModel.set(ITEM_2_TITLE, mItem2Provider.getCardTitle());
            mModel.set(ITEM_2_DESCRIPTION, mItem2Provider.getCardDescription());
            mModel.set(ITEM_2_ICON, mItem2Provider.getCardImage());
            mModel.set(ITEM_2_CLICK_HANDLER, mItem2Provider::onCardClicked);
        }
    }

    // ModuleProvider implementation.
    @Override
    public void showModule() {
        mModuleDelegate.onDataReady(
                ModuleDelegate.ModuleType.SETUP_LIST_TWO_CELL_CONTAINER, mModel);
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
        return ModuleDelegate.ModuleType.SETUP_LIST_TWO_CELL_CONTAINER;
    }
}
