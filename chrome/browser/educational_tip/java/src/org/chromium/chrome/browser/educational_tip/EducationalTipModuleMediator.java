// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the educational tip module. */
public class EducationalTipModuleMediator {
    @VisibleForTesting static final String FORCE_TAB_GROUP = "force_tab_group";
    @VisibleForTesting static final String FORCE_TAB_GROUP_SYNC = "force_tab_group_sync";
    @VisibleForTesting static final String FORCE_QUICK_DELETE = "force_quick_delete";
    @VisibleForTesting static final String FORCE_DEFAULT_BROWSER = "force_default_browser";

    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Profile mProfile;
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
        mProfile = getRegularProfile(mActionDelegate.getProfileSupplier());

        mCallbackController = new CallbackController();
    }

    /** Show the educational tip module. */
    void showModule() {
        @EducationalTipCardType Integer forcedCardType = getForcedCardType();
        if (forcedCardType != null) {
            showModuleWithCardInfo(forcedCardType);
        } else if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER)) {
            getCardTypeFromSegmentation();
        } else {
            // The educational tip module doesn’t display any card when no card is forced to show
            // and the ephemeral card ranker for the segmentation platform service is disabled.
            showModuleWithCardInfo(/* cardType= */ null);
        }
    }

    /** Called when the educational tip module is visible to users on the magic stack. */
    void onViewCreated() {
        EducationalTipModuleMediatorJni.get()
                .notifyCardShown(mProfile, mEducationalTipCardProvider.getCardType());
    }

    @VisibleForTesting
    void showModuleWithCardInfo(@Nullable Integer cardType) {
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
    private @Nullable Integer getForcedCardType() {
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

    private void getCardTypeFromSegmentation() {
        SegmentationPlatformService segmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(mProfile);

        segmentationPlatformService.getClassificationResult(
                "ephemeral_home_module_backend",
                /* prediction_options= */ createPredictionOptions(),
                /* inputContext= */ createInputContext(),
                result -> {
                    showModuleWithCardInfo(onGetClassificationResult(result));
                });
    }

    /** Creates an instance of InputContext. */
    @VisibleForTesting
    InputContext createInputContext() {
        InputContext inputContext = new InputContext();
        inputContext.addEntry(
                "is_default_browser_chrome", ProcessedValue.fromFloat(isDefaultBrowserChrome()));
        inputContext.addEntry(
                "has_default_browser_promo_reached_limit_in_role_manager",
                ProcessedValue.fromFloat(hasDefaultBrowserPromoReachedLimitInRoleManager()));
        return inputContext;
    }

    /** Creates an instance of PredictionOptions. */
    @VisibleForTesting
    PredictionOptions createPredictionOptions() {
        return new PredictionOptions(/* onDemandExecution= */ true);
    }

    @VisibleForTesting
    @Nullable
    Integer onGetClassificationResult(ClassificationResult result) {
        // If segmentation service fails, not show any card.
        if (result.status != PredictionStatus.SUCCEEDED || result.orderedLabels.isEmpty()) {
            return null;
        } else {
            Integer cardType =
                    EducationalTipCardProvider.convertLabelToCardType(result.orderedLabels.get(0));
            return cardType;
        }
    }

    private float isDefaultBrowserChrome() {
        // TODO(crbug.com/355015904): add trigger scenarios here for default browser promo card.
        return 0.0f;
    }

    private float hasDefaultBrowserPromoReachedLimitInRoleManager() {
        // TODO(crbug.com/355015904): add trigger scenarios here for default browser promo card.
        return 1.0f;
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
        // TODO(crbug.com/355015904): Records metrics for clicking the card.
    }

    /** Gets the regular profile if exists. */
    private Profile getRegularProfile(ObservableSupplier<Profile> profileSupplier) {
        assert profileSupplier.hasValue();

        return profileSupplier.get().getOriginalProfile();
    }

    @NativeMethods
    public interface Natives {
        void notifyCardShown(@JniType("Profile*") Profile profile, int cardType);
    }
}
