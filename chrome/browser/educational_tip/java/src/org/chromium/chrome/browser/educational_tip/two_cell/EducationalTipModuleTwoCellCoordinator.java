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
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.SEE_MORE_CLICK_HANDLER;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProviderFactory;
import org.chromium.chrome.browser.educational_tip.EducationalTipModuleUtils;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
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
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final PropertyModel mModel;
    private final CallbackController mCallbackController = new CallbackController();
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final BottomSheetObserver mBottomSheetObserver;
    private @Nullable EducationalTipCardProvider mItem1Provider;
    private @Nullable EducationalTipCardProvider mItem2Provider;
    private @ModuleType int mItem1Type;
    private @ModuleType int mItem2Type;

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
        mActionDelegate = actionDelegate;

        mModel = new PropertyModel.Builder(EducationalTipModuleTwoCellProperties.ALL_KEYS).build();
        mModel.set(
                MODULE_TITLE,
                mActionDelegate.getContext().getString(R.string.educational_tip_module_title));

        mBottomSheetObserver =
                EducationalTipModuleUtils.createBottomSheetObserver(
                        () ->
                                mItem1Type == ModuleType.DEFAULT_BROWSER_PROMO
                                        || mItem2Type == ModuleType.DEFAULT_BROWSER_PROMO,
                        this::updateModule);
        mActionDelegate.getBottomSheetController().addObserver(mBottomSheetObserver);

        refreshSlots();
    }

    /**
     * Re-queries the ranked module list and updates the providers and model for the two slots based
     * on the top items.
     */
    private void refreshSlots() {
        List<Integer> setupListModuleTypes = SetupListModuleUtils.getRankedModuleTypes();
        assert setupListModuleTypes.size() >= 2 : "Two cell layout requires at least two items";

        mItem1Type = setupListModuleTypes.get(0);
        mItem2Type = setupListModuleTypes.get(1);

        EducationalTipBottomSheetCoordinator educationalTipBottomSheetCoordinator =
                new EducationalTipBottomSheetCoordinator(mActionDelegate);
        mModel.set(SEE_MORE_CLICK_HANDLER, educationalTipBottomSheetCoordinator::showBottomSheet);

        Runnable removeModuleCallback = () -> mModuleDelegate.removeModule(getModuleType());

        // Destroy previous providers if they exist.
        if (mItem1Provider != null) mItem1Provider.destroy();
        if (mItem2Provider != null) mItem2Provider.destroy();

        // Refresh Slot 1
        mItem1Provider =
                EducationalTipCardProviderFactory.createInstance(
                        mItem1Type,
                        () -> {
                            mModuleDelegate.onModuleClicked(mModuleType);
                            SetupListModuleUtils.setModuleCompleted(mItem1Type);
                        },
                        mCallbackController,
                        mActionDelegate,
                        removeModuleCallback);
        if (mItem1Provider != null) {
            mModel.set(ITEM_1_TITLE, mItem1Provider.getCardTitle());
            mModel.set(ITEM_1_DESCRIPTION, mItem1Provider.getCardDescription());
            mModel.set(ITEM_1_CLICK_HANDLER, mItem1Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(mItem1Provider), mItem1Type);
            if (completionState == null) {
                mModel.set(ITEM_1_ICON, mItem1Provider.getCardImage());
                mModel.set(ITEM_1_MARK_COMPLETED, false);
            } else {
                mModel.set(ITEM_1_MARK_COMPLETED, completionState.isCompleted);
                mModel.set(ITEM_1_ICON, completionState.iconRes);
            }
        }

        // Refresh Slot 2
        mItem2Provider =
                EducationalTipCardProviderFactory.createInstance(
                        mItem2Type,
                        () -> {
                            mModuleDelegate.onModuleClicked(mModuleType);
                            SetupListModuleUtils.setModuleCompleted(mItem2Type);
                        },
                        mCallbackController,
                        mActionDelegate,
                        removeModuleCallback);
        if (mItem2Provider != null) {
            mModel.set(ITEM_2_TITLE, mItem2Provider.getCardTitle());
            mModel.set(ITEM_2_DESCRIPTION, mItem2Provider.getCardDescription());
            mModel.set(ITEM_2_CLICK_HANDLER, mItem2Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState2 =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(mItem2Provider), mItem2Type);
            if (completionState2 == null) {
                mModel.set(ITEM_2_ICON, mItem2Provider.getCardImage());
                mModel.set(ITEM_2_MARK_COMPLETED, false);
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
        mActionDelegate.getBottomSheetController().removeObserver(mBottomSheetObserver);
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
    public void updateModule() {
        Profile profile = mActionDelegate.getProfileSupplier().get();
        if (profile != null) {
            SetupListManager.getInstance().maybePrimeCompletionStatus(profile.getOriginalProfile());
        }

        boolean item1NeedsAnimation =
                SetupListModuleUtils.isModuleAwaitingCompletionAnimation(mItem1Type);
        boolean item2NeedsAnimation =
                SetupListModuleUtils.isModuleAwaitingCompletionAnimation(mItem2Type);

        if (!item1NeedsAnimation && !item2NeedsAnimation) return;

        // 1. Immediately trigger the visual "completed" state for affected slots.
        if (item1NeedsAnimation) {
            mModel.set(ITEM_1_MARK_COMPLETED, true);
            if (mItem1Provider instanceof SetupListCompletable completable) {
                mModel.set(ITEM_1_ICON, completable.getCardImageCompletedResId());
            }
        }
        if (item2NeedsAnimation) {
            mModel.set(ITEM_2_MARK_COMPLETED, true);
            if (mItem2Provider instanceof SetupListCompletable completable) {
                mModel.set(ITEM_2_ICON, completable.getCardImageCompletedResId());
            }
        }

        // Wait for transition and delay, then move the module to the end of the Magic Stack.
        mHandler.postDelayed(
                mCallbackController.makeCancelable(
                        () -> {
                            if (item1NeedsAnimation) {
                                SetupListModuleUtils.finishCompletionAnimation(mItem1Type);
                            }
                            if (item2NeedsAnimation) {
                                SetupListModuleUtils.finishCompletionAnimation(mItem2Type);
                            }

                            // Re-query ranking and update slots with new top items.
                            refreshSlots();
                        }),
                SetupListManager.STRIKETHROUGH_DURATION_MS + SetupListManager.HIDE_DURATION_MS);
    }

    @Override
    public int getModuleType() {
        return mModuleType;
    }
}
