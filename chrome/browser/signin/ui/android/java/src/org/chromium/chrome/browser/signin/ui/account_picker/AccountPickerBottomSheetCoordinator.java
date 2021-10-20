// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.base.WindowAndroid;
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
        public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
            super.onSheetStateChanged(newState, reason);
            if (newState != BottomSheetController.SheetState.HIDDEN) {
                return;
            }

            if (reason == StateChangeReason.SWIPE) {
                logMetricAndIncrementActiveDismissalCount(
                        AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN);
            } else if (reason == StateChangeReason.BACK_PRESS) {
                logMetricAndIncrementActiveDismissalCount(
                        AccountConsistencyPromoAction.DISMISSED_BACK);
            } else if (reason == StateChangeReason.TAP_SCRIM) {
                logMetricAndIncrementActiveDismissalCount(
                        AccountConsistencyPromoAction.DISMISSED_SCRIM);
            }

            AccountPickerBottomSheetCoordinator.this.destroy();
        }
    };

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottom sheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(AccountConsistencyPromoAction.SHOWN);

        mAccountPickerBottomSheetMediator = new AccountPickerBottomSheetMediator(
                windowAndroid, accountPickerDelegate, this::onDismissButtonClicked);
        mView = new AccountPickerBottomSheetView(
                windowAndroid.getActivity().get(), mAccountPickerBottomSheetMediator);
        mAccountPickerCoordinator = new AccountPickerCoordinator(
                mView.getAccountListView(), mAccountPickerBottomSheetMediator);

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
    private void onDismissButtonClicked() {
        logMetricAndIncrementActiveDismissalCount(AccountConsistencyPromoAction.DISMISSED_BUTTON);
        mBottomSheetController.hideContent(mView, true);
    }

    @MainThread
    private void logMetricAndIncrementActiveDismissalCount(
            @AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction);
        SigninPreferencesManager.getInstance()
                .incrementAccountPickerBottomSheetActiveDismissalCount();
    }

    @VisibleForTesting
    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
