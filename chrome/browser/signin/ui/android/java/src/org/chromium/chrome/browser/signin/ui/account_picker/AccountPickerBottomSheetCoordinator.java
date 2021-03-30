// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialCoordinator;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialDelegate;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the account picker bottom sheet used in web signin flow.
 */
public class AccountPickerBottomSheetCoordinator {
    private final AccountPickerBottomSheetView mView;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            super.onSheetClosed(reason);
            if (reason == StateChangeReason.SWIPE) {
                logOnDismissMetrics(AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN);
            } else if (reason == StateChangeReason.BACK_PRESS) {
                logOnDismissMetrics(AccountConsistencyPromoAction.DISMISSED_BACK);
            } else if (reason == StateChangeReason.TAP_SCRIM) {
                logOnDismissMetrics(AccountConsistencyPromoAction.DISMISSED_SCRIM);
            }
        }

        @Override
        public void onSheetStateChanged(int newState) {
            super.onSheetStateChanged(newState);
            if (newState == BottomSheetController.SheetState.HIDDEN) {
                AccountPickerBottomSheetCoordinator.this.destroy();
            }
        }
    };

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottom sheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(Activity activity,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate, TabModel regularTabModel,
            TabCreator incognitoTabCreator, HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            boolean showIncognitoRow) {
        this(activity, bottomSheetController, accountPickerDelegate,
                new IncognitoInterstitialDelegate(
                        activity, regularTabModel, incognitoTabCreator, helpAndFeedbackLauncher),
                showIncognitoRow);
    }

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottom sheet on the screen.
     */
    @VisibleForTesting
    @MainThread
    public AccountPickerBottomSheetCoordinator(Activity activity,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            IncognitoInterstitialDelegate incognitoInterstitialDelegate, boolean showIncognitoRow) {
        SigninPreferencesManager.getInstance().incrementAccountPickerBottomSheetShownCount();
        SigninMetricsUtils.logAccountConsistencyPromoAction(AccountConsistencyPromoAction.SHOWN);
        SigninMetricsUtils.logAccountConsistencyPromoShownCount(
                "Signin.AccountConsistencyPromoAction.Shown.Count");

        mAccountPickerBottomSheetMediator = new AccountPickerBottomSheetMediator(
                activity, accountPickerDelegate, this::dismissBottomSheet);
        mView = new AccountPickerBottomSheetView(activity, mAccountPickerBottomSheetMediator);
        mAccountPickerCoordinator = new AccountPickerCoordinator(mView.getAccountListView(),
                mAccountPickerBottomSheetMediator, /* selectedAccountName= */ null,
                /* showIncognitoRow= */ showIncognitoRow);

        if (showIncognitoRow) {
            IncognitoInterstitialCoordinator incognitoInterstitialCoordinator =
                    new IncognitoInterstitialCoordinator(mView.getIncognitoInterstitialView(),
                            incognitoInterstitialDelegate, () -> {
                                SigninMetricsUtils.logAccountConsistencyPromoAction(
                                        AccountConsistencyPromoAction.STARTED_INCOGNITO_SESSION);
                            });
        }
        mBottomSheetController = bottomSheetController;
        PropertyModelChangeProcessor.create(mAccountPickerBottomSheetMediator.getModel(), mView,
                AccountPickerBottomSheetViewBinder::bind);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(mView, true);
    }

    /**
     * Releases the resources used by AccountPickerBottomSheetCoordinator.
     */
    @MainThread
    private void destroy() {
        mAccountPickerCoordinator.destroy();
        mAccountPickerBottomSheetMediator.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @MainThread
    private void dismissBottomSheet() {
        logOnDismissMetrics(AccountConsistencyPromoAction.DISMISSED_BUTTON);
        mBottomSheetController.hideContent(mView, true);
    }

    @MainThread
    private void logOnDismissMetrics(@AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction);
        SigninPreferencesManager.getInstance()
                .incrementAccountPickerBottomSheetActiveDismissalCount();
        SigninMetricsUtils.logWebSignin();
    }

    @VisibleForTesting
    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
