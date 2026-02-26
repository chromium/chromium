// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/** Mediator for the educational tip module. */
@NullMarked
public class EducationalTipModuleMediator {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private @ModuleType int mModuleType;
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final CallbackController mCallbackController;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final BottomSheetObserver mBottomSheetObserver;

    private @Nullable EducationalTipCardProvider mEducationalTipCardProvider;
    private final DefaultBrowserPromoTriggerStateListener mDefaultBrowserPromoTriggerStateListener;
    private final Tracker mTracker;

    EducationalTipModuleMediator(
            @ModuleType int moduleType,
            PropertyModel model,
            ModuleDelegate moduleDelegate,
            EducationTipModuleActionDelegate actionDelegate,
            Profile profile) {
        mModuleType = moduleType;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mActionDelegate = actionDelegate;
        mTracker = TrackerFactory.getTrackerForProfile(profile);
        mDefaultBrowserPromoTriggerStateListener = this::removeModule;

        mBottomSheetObserver =
                EducationalTipModuleUtils.createBottomSheetObserver(
                        () -> mModuleType == ModuleType.DEFAULT_BROWSER_PROMO, this::updateModule);
        mActionDelegate.getBottomSheetController().addObserver(mBottomSheetObserver);

        mCallbackController = new CallbackController();
    }

    /** Show the educational tip module. */
    void showModule() {
        if (mModuleType == ModuleType.DEFAULT_BROWSER_PROMO) {
            DefaultBrowserPromoUtils.getInstance()
                    .addListener(mDefaultBrowserPromoTriggerStateListener);
        }
        Runnable removeModuleCallback = () -> mModuleDelegate.removeModule(mModuleType);

        if (mModuleType == ModuleType.HISTORY_SYNC_PROMO
                && SetupListModuleUtils.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)) {
            removeModuleCallback = this::updateModule;
        }

        mEducationalTipCardProvider =
                EducationalTipCardProviderFactory.createInstance(
                        mModuleType,
                        this::onCardClicked,
                        mCallbackController,
                        mActionDelegate,
                        removeModuleCallback);
        assumeNonNull(mEducationalTipCardProvider);

        mModel.set(
                EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING,
                mEducationalTipCardProvider.getCardTitle());
        mModel.set(
                EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING,
                mEducationalTipCardProvider.getCardDescription());
        mModel.set(
                EducationalTipModuleProperties.MODULE_BUTTON_STRING,
                mEducationalTipCardProvider.getCardButtonText());

        // SetupListCompletable.getCompletionState() internally checks if the module is part of the
        // Setup List and returns the appropriate icon and completion status. For non-Setup List
        // modules, it defaults to the provider's image and isCompleted = false.
        SetupListCompletable.CompletionState completionState =
                SetupListCompletable.getCompletionState(
                        Objects.requireNonNull(mEducationalTipCardProvider), mModuleType);
        if (completionState == null) {
            mModel.set(
                    EducationalTipModuleProperties.MODULE_CONTENT_IMAGE,
                    mEducationalTipCardProvider.getCardImage());
        } else {
            mModel.set(EducationalTipModuleProperties.MARK_COMPLETED, completionState.isCompleted);
            mModel.set(
                    EducationalTipModuleProperties.MODULE_CONTENT_IMAGE, completionState.iconRes);
        }
        mModel.set(
                EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER,
                v -> assumeNonNull(mEducationalTipCardProvider).onCardClicked());

        mModuleDelegate.onDataReady(mModuleType, mModel);
    }

    /** Called when the educational tip module is visible to users on the magic stack. */
    void onViewCreated() {
        if (mEducationalTipCardProvider != null) {
            mEducationalTipCardProvider.onViewCreated();
        }

        if (mModuleType == ModuleType.DEFAULT_BROWSER_PROMO) {
            // For the Setup List version, we notify the system immediately to ensure mutual
            // exclusion with other surfaces (banners on Messages/Settings pages).
            if (SetupListModuleUtils.isSetupListModule(getModuleType())) {
                notifyDefaultBrowserPromoVisible();
            } else {
                if (mTracker.isInitialized()) {
                    boolean shouldDisplay =
                            mTracker.shouldTriggerHelpUi(
                                    FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK);
                    if (shouldDisplay) {
                        notifyDefaultBrowserPromoVisible();
                    }
                } else {
                    notifyDefaultBrowserPromoVisible();
                    mTracker.addOnInitializedCallback(
                            (T) ->
                                    mTracker.shouldTriggerHelpUi(
                                            FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK));
                }
            }
        }

        if (SetupListModuleUtils.isSetupListModule(mModuleType)) {
            SetupListModuleUtils.recordSetupListImpression();
            SetupListModuleUtils.recordSetupListItemImpression(
                    mModuleType, SetupListModuleUtils.isModuleCompleted(mModuleType));
        }
    }

    @ModuleType
    int getModuleType() {
        return mModuleType;
    }

    /**
     * Updates the module's data if necessary. For Setup List modules, this orchestrates the
     * completion animation.
     */
    void updateModule() {
        Profile profile = mActionDelegate.getProfileSupplier().get();
        if (profile == null) return;

        SetupListManager.getInstance().maybePrimeCompletionStatus(profile.getOriginalProfile());

        if (!SetupListModuleUtils.isModuleAwaitingCompletionAnimation(mModuleType)) return;

        mModel.set(EducationalTipModuleProperties.MARK_COMPLETED, true);
        if (mEducationalTipCardProvider instanceof SetupListCompletable completable) {
            mModel.set(
                    EducationalTipModuleProperties.MODULE_CONTENT_COMPLETED_IMAGE,
                    completable.getCardImageCompletedResId());
        }

        // Wait for transition and delay, then move the module to the end of the Magic Stack.
        mHandler.postDelayed(
                mCallbackController.makeCancelable(
                        () -> {
                            SetupListModuleUtils.finishCompletionAnimation(mModuleType);
                            mModuleDelegate.maybeMoveModuleToTheEnd(mModuleType);
                            if (SetupListManager.getInstance().shouldShowCelebratoryPromo()) {
                                mModuleDelegate.refreshModules();
                            }
                        }),
                SetupListManager.STRIKETHROUGH_DURATION_MS + SetupListManager.HIDE_DURATION_MS);
    }

    void destroy() {
        mActionDelegate.getBottomSheetController().removeObserver(mBottomSheetObserver);
        removeDefaultBrowserPromoTriggerStateListener();
        if (mEducationalTipCardProvider != null) {
            mEducationalTipCardProvider.destroy();
            mEducationalTipCardProvider = null;
        }
        mCallbackController.destroy();
    }

    /**
     * Triggered when the user has viewed the default browser promo from another location, removing
     * the educational tip module from the magic stack.
     */
    private void removeModule() {
        mModuleDelegate.removeModule(mModuleType);
        removeDefaultBrowserPromoTriggerStateListener();
    }

    /**
     * Removes the {@link DefaultBrowserPromoTriggerStateListener} from the listener list in {@link
     * DefaultBrowserPromoUtils}.
     */
    private void removeDefaultBrowserPromoTriggerStateListener() {
        DefaultBrowserPromoUtils.getInstance()
                .removeListener(mDefaultBrowserPromoTriggerStateListener);
    }

    /** Called when user clicks the card. */
    private void onCardClicked() {
        mModuleDelegate.onModuleClicked(mModuleType);

        if (SetupListModuleUtils.isSetupListModule(mModuleType)) {
            // Considered complete if the user clicks on the promo
            SetupListModuleUtils.setModuleCompleted(mModuleType, /* silent= */ false);

            SetupListModuleUtils.recordSetupListClick();
            SetupListModuleUtils.recordSetupListItemClick(mModuleType);
        }
    }

    /** Notifies that the default browser promo is visible. */
    private void notifyDefaultBrowserPromoVisible() {
        DefaultBrowserPromoUtils defaultBrowserPromoUtils = DefaultBrowserPromoUtils.getInstance();
        defaultBrowserPromoUtils.removeListener(mDefaultBrowserPromoTriggerStateListener);
        defaultBrowserPromoUtils.notifyDefaultBrowserPromoVisible();
    }

    DefaultBrowserPromoTriggerStateListener getDefaultBrowserPromoTriggerStateListenerForTesting() {
        return mDefaultBrowserPromoTriggerStateListener;
    }

    @Nullable EducationalTipCardProvider getCardProviderForTesting() {
        return mEducationalTipCardProvider;
    }

    void setModuleTypeForTesting(@ModuleType int moduleType) {
        mModuleType = moduleType;
    }
}
