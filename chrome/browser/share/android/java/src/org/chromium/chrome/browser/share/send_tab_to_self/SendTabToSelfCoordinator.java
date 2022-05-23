// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.sync.ModelType;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Coordinator for displaying the send tab to self feature.
 */
public class SendTabToSelfCoordinator {
    /**
     * Waits for Sync to download the list of target devices after sign-in. Aborts if the
     * user dismisses the sign-in bottom sheet ("account picker") before success.
     */
    private static class TargetDeviceListWaiter
            extends EmptyBottomSheetObserver implements SyncService.SyncStateChangedListener {
        private final BottomSheetController mBottomSheetController;
        private final Runnable mGotDeviceListCallback;

        /**
         * Note there's no need for a notion for a failure callback because in that case the
         * account picker bottom sheet was closed and there's nothing left to do (simply don't
         * show any other bottom sheet).
         */
        public TargetDeviceListWaiter(
                BottomSheetController bottomSheetController, Runnable gotDeviceListCallback) {
            mBottomSheetController = bottomSheetController;
            mGotDeviceListCallback = gotDeviceListCallback;

            SyncService.get().addSyncStateChangedListener(this);
            mBottomSheetController.addObserver(this);
            notifyAndDestroyIfDone();
        }

        private void destroy() {
            SyncService.get().removeSyncStateChangedListener(this);
            mBottomSheetController.removeObserver(this);
        }

        @Override
        public void syncStateChanged() {
            notifyAndDestroyIfDone();
        }

        @Override
        public void onSheetClosed(int reason) {
            // The account picker doesn't dismiss itself, so this must mean the user did.
            destroy();
        }

        private void notifyAndDestroyIfDone() {
            if (SyncService.get().getActiveDataTypes().contains(ModelType.DEVICE_INFO)) {
                destroy();
                mGotDeviceListCallback.run();
            }
        }
    }

    /** Performs sign-in for the promo shown to signed-out users. */
    private static class SendTabToSelfAccountPickerDelegate implements AccountPickerDelegate {
        private final Runnable mOnSignInCompleteCallback;

        public SendTabToSelfAccountPickerDelegate(Runnable onSignInCompleteCallback) {
            mOnSignInCompleteCallback = onSignInCompleteCallback;
        }

        @Override
        public void destroy() {}

        @Override
        public void signIn(
                String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            Account account = AccountUtils.createAccountFromName(accountEmail);
            signinManager.signin(account, new SigninManager.SignInCallback() {
                @Override
                public void onSignInComplete() {
                    mOnSignInCompleteCallback.run();
                }

                @Override
                public void onSignInAborted() {
                    // TODO(crbug.com/1219434) Consider calling onSignInErrorCallback here.
                }
            });
        }

        @Override
        @EntryPoint
        public int getEntryPoint() {
            return EntryPoint.SEND_TAB_TO_SELF;
        }
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final String mUrl;
    private final String mTitle;
    private final BottomSheetController mController;

    public SendTabToSelfCoordinator(Context context, WindowAndroid windowAndroid, String url,
            String title, BottomSheetController controller) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mUrl = url;
        mTitle = title;
        mController = controller;
    }

    public void show() {
        if (!shouldOfferSignInPromo()) {
            showDeviceList();
            return;
        }

        new AccountPickerBottomSheetCoordinator(mWindowAndroid, mController,
                new SendTabToSelfAccountPickerDelegate(this::onSignInComplete));
    }

    private void onSignInComplete() {
        new TargetDeviceListWaiter(mController, this::onTargetDeviceListReady);
    }

    private void onTargetDeviceListReady() {
        mController.hideContent(mController.getCurrentSheetContent(), /*animate=*/true);
        showDeviceList();
    }

    private void showDeviceList() {
        mController.requestShowContent(
                new DevicePickerBottomSheetContent(mContext, mUrl, mTitle, mController), true);
    }

    private boolean shouldOfferSignInPromo() {
        // There should be some account on the device that can sign in to Chrome.
        List<Account> accounts = AccountUtils.getAccountsIfFulfilledOrEmpty(
                AccountManagerFacadeProvider.getInstance().getAccounts());
        if (accounts.isEmpty()) {
            return false;
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        if (!IdentityServicesProvider.get().getSigninManager(profile).isSigninAllowed()) {
            return false;
        }

        // There should be no account signed in to Chrome yet.
        if (SyncService.get().getAccountInfo() != null) {
            return false;
        }

        return ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO);
    }
}
