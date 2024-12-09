// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MAGIC_STACK_MODULE;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_PREFIX;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.PredictionOptions;
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
    private DefaultBrowserPromoTriggerStateListener mDefaultBrowserPromoTriggerStateListener;
    private Tracker mTracker;

    EducationalTipModuleMediator(
            @NonNull PropertyModel model,
            @NonNull ModuleDelegate moduleDelegate,
            EducationTipModuleActionDelegate actionDelegate) {
        mModuleType = ModuleType.EDUCATIONAL_TIP;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mActionDelegate = actionDelegate;
        mProfile = getRegularProfile(mActionDelegate.getProfileSupplier());
        mTracker = TrackerFactory.getTrackerForProfile(mProfile);
        mDefaultBrowserPromoTriggerStateListener = this::removeModule;

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
        @EducationalTipCardType int cardType = mEducationalTipCardProvider.getCardType();

        // TODO(crbug.com/382803396): The sample here is a temporary workaround and will need to be
        // fully replaced with the module type after the refactor.
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_PREFIX
                        + HISTOGRAM_MAGIC_STACK_MODULE
                        + HomeModulesMetricsUtils.getModuleName(mModuleType)
                        + HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION,
                cardType + ModuleType.NUM_ENTRIES,
                EducationalTipCardType.NUM_ENTRIES + ModuleType.NUM_ENTRIES);

        if (cardType == EducationalTipCardType.DEFAULT_BROWSER_PROMO) {
            EducationalTipModuleMediatorJni.get().notifyCardShown(mProfile, cardType);

            boolean shouldDisplay =
                    mTracker.shouldTriggerHelpUi(
                            FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK);
            // The shouldDisplay flag should be true in this situation because if the other default
            // browser promotion is visible to the user, this educational tip module should already
            // be hidden.
            if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.EDUCATIONAL_TIP_MODULE, FORCE_DEFAULT_BROWSER, false)) {
                assert shouldDisplay;
            }

            DefaultBrowserPromoUtils defaultBrowserPromoUtils =
                    DefaultBrowserPromoUtils.getInstance();
            defaultBrowserPromoUtils.removeListener(mDefaultBrowserPromoTriggerStateListener);
            defaultBrowserPromoUtils.notifyDefaultBrowserPromoVisible();
            return;
        }

        EducationalTipCardProviderSignalHandler educationalTipCardProviderSignalHandler =
                EducationalTipCardProviderSignalHandler.getInstance();
        if (educationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(cardType)) {
            EducationalTipModuleMediatorJni.get().notifyCardShown(mProfile, cardType);
        }
    }

    @VisibleForTesting
    void showModuleWithCardInfo(@Nullable Integer cardType) {
        if (cardType == null) {
            mModuleDelegate.onDataFetchFailed(mModuleType);
            return;
        }

        if (cardType == EducationalTipCardType.DEFAULT_BROWSER_PROMO) {
            DefaultBrowserPromoUtils.getInstance()
                    .addListener(mDefaultBrowserPromoTriggerStateListener);
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
            return EducationalTipCardType.TAB_GROUP;
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
                /* inputContext= */ EducationalTipCardProviderSignalHandler.getInstance()
                        .createInputContext(mActionDelegate, mTracker),
                result -> {
                    showModuleWithCardInfo(onGetClassificationResult(result));
                });
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

    @ModuleType
    int getModuleType() {
        return mModuleType;
    }

    void destroy() {
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
        // TODO(crbug.com/355015904): Notifies the segmentation platform and records metrics for
        // clicking the card.
        @EducationalTipCardType int cardType = mEducationalTipCardProvider.getCardType();
        EducationalTipModuleMediatorJni.get().notifyCardInteracted(mProfile, cardType);
    }

    /** Gets the regular profile if exists. */
    private Profile getRegularProfile(ObservableSupplier<Profile> profileSupplier) {
        assert profileSupplier.hasValue();

        return profileSupplier.get().getOriginalProfile();
    }

    DefaultBrowserPromoTriggerStateListener getDefaultBrowserPromoTriggerStateListenerForTesting() {
        return mDefaultBrowserPromoTriggerStateListener;
    }

    @NativeMethods
    public interface Natives {
        void notifyCardShown(@JniType("Profile*") Profile profile, int cardType);

        void notifyCardInteracted(@JniType("Profile*") Profile profile, int cardType);
    }
}
