// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialCoordinator;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialDelegate;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
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
                AccountPickerDelegate.recordAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN);
            } else if (reason == StateChangeReason.BACK_PRESS) {
                AccountPickerDelegate.recordAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.DISMISSED_BACK);
            } else if (reason == StateChangeReason.TAP_SCRIM) {
                AccountPickerDelegate.recordAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.DISMISSED_SCRIM);
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
            AccountPickerDelegate accountPickerDelegate,
            IncognitoInterstitialDelegate incognitoInterstitialDelegate) {
        SigninPreferencesManager.getInstance().incrementAccountPickerBottomSheetShownCount();
        AccountPickerDelegate.recordAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.SHOWN);
        AccountPickerDelegate.recordAccountConsistencyPromoShownCount(
                "Signin.AccountConsistencyPromoAction.Shown.Count");

        mAccountPickerBottomSheetMediator = new AccountPickerBottomSheetMediator(
                activity, accountPickerDelegate, this::dismissBottomSheet);
        mView = new AccountPickerBottomSheetView(activity, mAccountPickerBottomSheetMediator);
        mAccountPickerCoordinator = new AccountPickerCoordinator(mView.getAccountListView(),
                mAccountPickerBottomSheetMediator, /* selectedAccountName= */ null,
                /* showIncognitoRow= */ IncognitoUtils.isIncognitoModeEnabled());
        IncognitoInterstitialCoordinator incognitoInterstitialCoordinator =
                new IncognitoInterstitialCoordinator(
                        mView.getIncognitoInterstitialView(), incognitoInterstitialDelegate);
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
        mBottomSheetController.hideContent(mView, true);
    }

    @VisibleForTesting
    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
