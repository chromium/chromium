// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
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
    /** Waits for Sync to download the list of target devices after sign-in. */
    private static class TargetDeviceListWaiter implements SyncService.SyncStateChangedListener {
        private final Promise<Void> mPromise = new Promise<Void>();

        public TargetDeviceListWaiter() {
            SyncService.get().addSyncStateChangedListener(this);
            fullfillIfReady();
        }

        public Promise<Void> waitUntilReady() {
            return mPromise;
        }

        @Override
        public void syncStateChanged() {
            fullfillIfReady();
        }

        private void fullfillIfReady() {
            if (SyncService.get().getActiveDataTypes().contains(ModelType.DEVICE_INFO)) {
                SyncService.get().removeSyncStateChangedListener(this);
                mPromise.fulfill(null);
            }
        }
    }

    /** Performs sign-in for the promo shown to signed-out users. */
    private static class SendTabToSelfAccountPickerDelegate implements AccountPickerDelegate {
        private final Runnable mShowDeviceListCallback;

        public SendTabToSelfAccountPickerDelegate(Runnable showDeviceListCallback) {
            mShowDeviceListCallback = showDeviceListCallback;
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
                    new TargetDeviceListWaiter().waitUntilReady().then(
                            unused -> { mShowDeviceListCallback.run(); });
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
    private final long mNavigationTime;

    public SendTabToSelfCoordinator(Context context, WindowAndroid windowAndroid, String url,
            String title, BottomSheetController controller, long navigationTime) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mUrl = url;
        mTitle = title;
        mController = controller;
        mNavigationTime = navigationTime;
    }

    public void show() {
        if (!shouldOfferSignInPromo()) {
            showDeviceList();
            return;
        }

        Runnable showDeviceListCallback = () -> {
            // TODO(crbug.com/1219434): The sign-in promo should close itself instead.
            mController.hideContent(mController.getCurrentSheetContent(), /*animate=*/true);
            showDeviceList();
        };
        new AccountPickerBottomSheetCoordinator(mWindowAndroid, mController,
                new SendTabToSelfAccountPickerDelegate(showDeviceListCallback));
    }

    private void showDeviceList() {
        mController.requestShowContent(new DevicePickerBottomSheetContent(mContext, mUrl, mTitle,
                                               mNavigationTime, mController),
                true);
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
