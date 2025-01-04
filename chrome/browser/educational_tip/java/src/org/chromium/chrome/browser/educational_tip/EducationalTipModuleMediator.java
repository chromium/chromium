// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the educational tip module. */
public class EducationalTipModuleMediator {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Profile mProfile;
    private @ModuleType int mModuleType;
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final CallbackController mCallbackController;

    private EducationalTipCardProvider mEducationalTipCardProvider;
    private DefaultBrowserPromoTriggerStateListener mDefaultBrowserPromoTriggerStateListener;
    private Tracker mTracker;

    EducationalTipModuleMediator(
            @ModuleType int moduleType,
            @NonNull PropertyModel model,
            @NonNull ModuleDelegate moduleDelegate,
            EducationTipModuleActionDelegate actionDelegate,
            @NonNull Profile profile) {
        mModuleType = moduleType;
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mActionDelegate = actionDelegate;
        mProfile = profile;
        mTracker = TrackerFactory.getTrackerForProfile(mProfile);
        mDefaultBrowserPromoTriggerStateListener = this::removeModule;

        mCallbackController = new CallbackController();
    }

    /** Show the educational tip module. */
    void showModule() {
        if (mModuleType == ModuleType.DEFAULT_BROWSER_PROMO) {
            DefaultBrowserPromoUtils.getInstance()
                    .addListener(mDefaultBrowserPromoTriggerStateListener);
        }

        mEducationalTipCardProvider =
                EducationalTipCardProviderFactory.createInstance(
                        mModuleType, this::onCardClicked, mCallbackController, mActionDelegate);

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

    /** Called when the educational tip module is visible to users on the magic stack. */
    void onViewCreated() {
        if (mModuleType == ModuleType.DEFAULT_BROWSER_PROMO) {
            boolean shouldDisplay =
                    mTracker.shouldTriggerHelpUi(
                            FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK);
            if (shouldDisplay) {
                DefaultBrowserPromoUtils defaultBrowserPromoUtils =
                        DefaultBrowserPromoUtils.getInstance();
                defaultBrowserPromoUtils.removeListener(mDefaultBrowserPromoTriggerStateListener);
                defaultBrowserPromoUtils.notifyDefaultBrowserPromoVisible();
            }
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
        mModuleDelegate.onModuleClicked(mModuleType);
    }

    DefaultBrowserPromoTriggerStateListener getDefaultBrowserPromoTriggerStateListenerForTesting() {
        return mDefaultBrowserPromoTriggerStateListener;
    }

    void setModuleTypeForTesting(@ModuleType int moduleType) {
        mModuleType = moduleType;
    }
}
