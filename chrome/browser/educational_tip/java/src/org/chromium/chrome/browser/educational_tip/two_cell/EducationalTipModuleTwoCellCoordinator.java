// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_COMPLETED_ICON;
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
import org.chromium.build.annotations.NonNull;
import org.chromium.build.annotations.NullMarked;
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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

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
    private final Map<Integer, EducationalTipCardProvider> mProviders = new HashMap<>();
    private List<Integer> mCurrentRankedModuleTypes = new ArrayList<>();
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
        if (!SetupListModuleUtils.shouldShowTwoCellLayout()) {
            return;
        }
        mCurrentRankedModuleTypes = SetupListModuleUtils.getRankedModuleTypes();
        if (mCurrentRankedModuleTypes.size() < 2) {
            return;
        }

        refreshProviders();

        mItem1Type = mCurrentRankedModuleTypes.get(0);
        mItem2Type = mCurrentRankedModuleTypes.get(1);

        EducationalTipCardProvider item1Provider = mProviders.get(mItem1Type);
        EducationalTipCardProvider item2Provider = mProviders.get(mItem2Type);

        EducationalTipBottomSheetCoordinator educationalTipBottomSheetCoordinator =
                getEducationalTipBottomSheetCoordinator();
        mModel.set(SEE_MORE_CLICK_HANDLER, educationalTipBottomSheetCoordinator::showBottomSheet);

        // Refresh Slot 1
        if (item1Provider != null) {
            mModel.set(ITEM_1_TITLE, item1Provider.getCardTitle());
            mModel.set(ITEM_1_DESCRIPTION, item1Provider.getCardDescription());
            mModel.set(ITEM_1_CLICK_HANDLER, item1Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(item1Provider), mItem1Type);
            if (completionState == null) {
                mModel.set(ITEM_1_ICON, item1Provider.getCardImage());
                mModel.set(ITEM_1_MARK_COMPLETED, false);
            } else {
                mModel.set(ITEM_1_MARK_COMPLETED, completionState.isCompleted);
                mModel.set(ITEM_1_ICON, completionState.iconRes);
            }
        }

        // Refresh Slot 2
        if (item2Provider != null) {
            mModel.set(ITEM_2_TITLE, item2Provider.getCardTitle());
            mModel.set(ITEM_2_DESCRIPTION, item2Provider.getCardDescription());
            mModel.set(ITEM_2_CLICK_HANDLER, item2Provider::onCardClicked);

            SetupListCompletable.CompletionState completionState2 =
                    SetupListCompletable.getCompletionState(
                            Objects.requireNonNull(item2Provider), mItem2Type);
            if (completionState2 == null) {
                mModel.set(ITEM_2_ICON, item2Provider.getCardImage());
                mModel.set(ITEM_2_MARK_COMPLETED, false);
            } else {
                mModel.set(ITEM_2_MARK_COMPLETED, completionState2.isCompleted);
                mModel.set(ITEM_2_ICON, completionState2.iconRes);
            }
        }
    }

    @NonNull
    private EducationalTipBottomSheetCoordinator getEducationalTipBottomSheetCoordinator() {
        Supplier<List<EducationalTipBottomSheetItem>> bottomSheetSupplier =
                () -> {
                    List<EducationalTipBottomSheetItem> output = new ArrayList<>();
                    for (@ModuleType int type : mCurrentRankedModuleTypes) {
                        EducationalTipCardProvider provider = mProviders.get(type);
                        if (provider != null) {
                            SetupListCompletable.CompletionState completionState =
                                    SetupListCompletable.getCompletionState(provider, type);
                            output.add(
                                    new EducationalTipBottomSheetItem(provider, completionState));
                        }
                    }
                    return output;
                };

        return new EducationalTipBottomSheetCoordinator(mActionDelegate, bottomSheetSupplier);
    }

    /**
     * Updates the map of providers, creating new ones for module types that have become eligible
     * and destroying those that are no longer in the ranked list.
     */
    private void refreshProviders() {
        // 1. Destroy and remove providers that are no longer in the ranked list.
        Iterator<Map.Entry<Integer, EducationalTipCardProvider>> it =
                mProviders.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<Integer, EducationalTipCardProvider> entry = it.next();
            if (mCurrentRankedModuleTypes.contains(entry.getKey())) {
                continue;
            }
            if (entry.getValue() != null) {
                entry.getValue().destroy();
            }
            it.remove();
        }

        // 2. Create missing providers.
        for (@ModuleType int type : mCurrentRankedModuleTypes) {
            if (mProviders.containsKey(type)) {
                continue;
            }
            mProviders.put(
                    type,
                    EducationalTipCardProviderFactory.createInstance(
                            type,
                            () -> {
                                mModuleDelegate.onModuleClicked(mModuleType);
                                SetupListModuleUtils.setModuleCompleted(type, /* silent= */ false);
                                SetupListModuleUtils.recordSetupListClick();
                                SetupListModuleUtils.recordSetupListItemClick(type);
                            },
                            mCallbackController,
                            mActionDelegate,
                            this::updateModule));
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
        for (EducationalTipCardProvider provider : mProviders.values()) {
            if (provider != null) {
                provider.destroy();
            }
        }
        mProviders.clear();
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

        Set<Integer> modulesAwaitingCompletionAnimation =
                SetupListManager.getInstance().getModulesAwaitingCompletionAnimation();
        boolean item1NeedsAnimation = modulesAwaitingCompletionAnimation.contains(mItem1Type);
        boolean item2NeedsAnimation = modulesAwaitingCompletionAnimation.contains(mItem2Type);

        // 1. Immediately finish animation for modules that are NOT in the visible slots.
        // They should reorder silently since the user can't see them anyway.
        for (Integer type : new ArrayList<>(modulesAwaitingCompletionAnimation)) {
            if (type != mItem1Type && type != mItem2Type) {
                SetupListManager.getInstance().onCompletionAnimationFinished(type);
            }
        }

        if (!item1NeedsAnimation && !item2NeedsAnimation) {
            refreshSlots();
            return;
        }

        // 2. Immediately trigger the visual "completed" state for affected slots.
        if (item1NeedsAnimation) {
            mModel.set(ITEM_1_MARK_COMPLETED, true);
            EducationalTipCardProvider item1Provider = mProviders.get(mItem1Type);
            if (item1Provider instanceof SetupListCompletable completable) {
                mModel.set(ITEM_1_COMPLETED_ICON, completable.getCardImageCompletedResId());
            }
        }
        if (item2NeedsAnimation) {
            mModel.set(ITEM_2_MARK_COMPLETED, true);
            EducationalTipCardProvider item2Provider = mProviders.get(mItem2Type);
            if (item2Provider instanceof SetupListCompletable completable) {
                mModel.set(ITEM_2_COMPLETED_ICON, completable.getCardImageCompletedResId());
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

                            if (SetupListManager.getInstance().shouldShowCelebratoryPromo()) {
                                mModuleDelegate.refreshModules();
                            }
                        }),
                SetupListManager.STRIKETHROUGH_DURATION_MS + SetupListManager.HIDE_DURATION_MS);
    }

    @Override
    public int getModuleType() {
        return mModuleType;
    }

    @Override
    public void onViewCreated() {
        for (EducationalTipCardProvider provider : mProviders.values()) {
            if (provider != null) {
                provider.onViewCreated();
            }
        }

        SetupListModuleUtils.recordSetupListImpression();
        SetupListModuleUtils.recordSetupListItemImpression(
                mItem1Type, SetupListModuleUtils.isModuleCompleted(mItem1Type));
        SetupListModuleUtils.recordSetupListItemImpression(
                mItem2Type, SetupListModuleUtils.isModuleCompleted(mItem2Type));
    }
}
