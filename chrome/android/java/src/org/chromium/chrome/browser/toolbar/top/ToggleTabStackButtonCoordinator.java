// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.FeatureParamUtils;
import org.chromium.chrome.browser.intent.IntentMetadata;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Root component for the tab switcher button on the toolbar. Intended to own the
 * {@link ToggleTabStackButton}, but currently it only manages some signals around the tab switcher
 * button.
 * TODO(https://crbug.com/871806): Finish converting HomeButton to MVC and move more logic into this
 * class.
 */
public class ToggleTabStackButtonCoordinator {
    @VisibleForTesting
    static final String MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME = "isMainIntentFromLauncher";
    @VisibleForTesting
    static final String INTENT_WITH_EFFECT_PARAM_NAME = "intentWithEffect";

    private final CallbackController mCallbackController = new CallbackController();
    private final Context mContext;
    private final ToggleTabStackButton mToggleTabStackButton;
    private final UserEducationHelper mUserEducationHelper;
    private final BooleanSupplier mIsIncognitoSupplier;
    private final OneshotSupplier<IntentMetadata> mIntentMetadataOneshotSupplier;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final Callback<Boolean> mSetNewTabButtonHighlightCallback;

    private OverviewModeBehavior mOverviewModeBehavior;
    private ActivityTabProvider.ActivityTabTabObserver mPageLoadObserver;
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private boolean mIphBeingShown;

    /**
     * @param context The Android context used for various view operations.
     * @param toggleTabStackButton The concrete {@link ToggleTabStackButton} class for this MVC
     *         component.
     * @param activityTabProvider Provides the current active tab.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param isIncognitoSupplier Supplier for whether the current tab is incognito.
     * @param intentMetadataOneshotSupplier Potentially delayed information about launching intent.
     * @param promoShownOneshotSupplier Potentially delayed information about if a promo was shown.
     * @param overviewModeBehaviorSupplier Allows observing overview mode state.
     * @param setNewTabButtonHighlightCallback Delegate to highlight the new tab button.
     *
     */
    public ToggleTabStackButtonCoordinator(Context context,
            ToggleTabStackButton toggleTabStackButton, ActivityTabProvider activityTabProvider,
            UserEducationHelper userEducationHelper, BooleanSupplier isIncognitoSupplier,
            OneshotSupplier<IntentMetadata> intentMetadataOneshotSupplier,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            Callback<Boolean> setNewTabButtonHighlightCallback) {
        mContext = context;
        mToggleTabStackButton = toggleTabStackButton;
        mUserEducationHelper = userEducationHelper;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mIntentMetadataOneshotSupplier = intentMetadataOneshotSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mSetNewTabButtonHighlightCallback = setNewTabButtonHighlightCallback;

        overviewModeBehaviorSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setOverviewModeBehavior));
        mPageLoadObserver = new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                handlePageLoadFinished();
            }
        };
    }

    /** Cleans up callbacks and observers. */
    public void destroy() {
        mCallbackController.destroy();

        if (mPageLoadObserver != null) {
            mPageLoadObserver.destroy();
            mPageLoadObserver = null;
        }

        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
            mOverviewModeObserver = null;
        }
    }

    private void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert overviewModeBehavior != null;
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            private boolean mHighlightedNewTabPageButton;

            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                if (mIphBeingShown) {
                    mSetNewTabButtonHighlightCallback.onResult(true);
                    mHighlightedNewTabPageButton = true;
                }
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                if (mHighlightedNewTabPageButton) {
                    mSetNewTabButtonHighlightCallback.onResult(false);
                    mHighlightedNewTabPageButton = false;
                }
            }
        };
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    // TODO(https://crbug.com/1133355): Reduce visibility once ActivityTabTabObserver is mockable.
    @VisibleForTesting
    void handlePageLoadFinished() {
        if (mToggleTabStackButton == null || !mToggleTabStackButton.isShown()) return;
        if (mIsIncognitoSupplier.getAsBoolean()) return;
        if (mPromoShownOneshotSupplier.get() == null || mPromoShownOneshotSupplier.get()) return;

        IntentMetadata intentMetadata = mIntentMetadataOneshotSupplier.get();
        if (intentMetadata == null) return;
        if (FeatureParamUtils.paramExistsAndDoesNotMatch(
                    FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                    MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME,
                    intentMetadata.getIsMainIntentFromLauncher())) {
            return;
        }
        if (FeatureParamUtils.paramExistsAndDoesNotMatch(
                    FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE, INTENT_WITH_EFFECT_PARAM_NAME,
                    intentMetadata.getIsIntentWithEffect())) {
            return;
        }

        mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(mContext.getResources(),
                FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE, R.string.iph_tab_switcher_text,
                R.string.iph_tab_switcher_accessibility_text)
                                                    .setAnchorView(mToggleTabStackButton)
                                                    .setOnShowCallback(this::handleShowCallback)
                                                    .setOnDismissCallback(
                                                            this::handleDismissCallback)
                                                    .build());
    }

    private void handleShowCallback() {
        assert mToggleTabStackButton != null;
        mIphBeingShown = true;
        mToggleTabStackButton.setHighlightDrawable(true);
    }

    private void handleDismissCallback() {
        assert mToggleTabStackButton != null;
        mIphBeingShown = false;
        mToggleTabStackButton.setHighlightDrawable(false);
    }
}