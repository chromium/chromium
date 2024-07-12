// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetMediator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.Optional;

/** Coordinator for displaying the send tab to self feature. */
public class SendTabToSelfCoordinator {
    /**
     * Waits for Sync to download the list of target devices after sign-in. Aborts if the
     * user dismisses the sign-in bottom sheet ("account picker") before success.
     */
    private static class TargetDeviceListWaiter extends EmptyBottomSheetObserver
            implements SyncService.SyncStateChangedListener {
        private final BottomSheetController mBottomSheetController;
        private final String mUrl;
        private final Runnable mGotDeviceListCallback;
        private final Profile mProfile;

        /**
         * Note there's no need for a notion for a failure callback because in that case the
         * account picker bottom sheet was closed and there's nothing left to do (simply don't
         * show any other bottom sheet).
         */
        public TargetDeviceListWaiter(
                BottomSheetController bottomSheetController,
                String url,
                Runnable gotDeviceListCallback,
                Profile profile) {
            mBottomSheetController = bottomSheetController;
            mUrl = url;
            mGotDeviceListCallback = gotDeviceListCallback;
            mProfile = profile;

            SyncServiceFactory.getForProfile(mProfile).addSyncStateChangedListener(this);
            mBottomSheetController.addObserver(this);
            notifyAndDestroyIfDone();
        }

        private void destroy() {
            SyncServiceFactory.getForProfile(mProfile).removeSyncStateChangedListener(this);
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
            Optional</*@EntryPointDisplayReason*/ Integer> displayReason =
                    SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
            // The model is starting up, keep waiting.
            if (!displayReason.isPresent()) return;

            switch (displayReason.get()) {
                case EntryPointDisplayReason.OFFER_SIGN_IN:
                    return;
                case EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE:
                case EntryPointDisplayReason.OFFER_FEATURE:
                    break;
            }

            destroy();
            mGotDeviceListCallback.run();
        }
    }

    /** Performs sign-in for the promo shown to signed-out users. */
    private static class SendTabToSelfAccountPickerDelegate implements AccountPickerDelegate {
        private final Runnable mOnSignInCompleteCallback;
        private final SigninManager mSigninManager;

        public SendTabToSelfAccountPickerDelegate(
                Runnable onSignInCompleteCallback, SigninManager signinManager) {
            mOnSignInCompleteCallback = onSignInCompleteCallback;
            mSigninManager = signinManager;
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public void onAccountPickerDestroy() {}

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public boolean canHandleAddAccount() {
            return false;
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public void addAccount() {
            // TODO(b/326019991): Remove this exception along with the delegate implementation once
            // all bottom sheet entry points will be started from `SigninAndHistorySyncActivity`.
            throw new UnsupportedOperationException(
                    "SendTabToSelfAccountPickerDelegate.addAccount() should never be called.");
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator) {
            mSigninManager.signin(
                    accountInfo,
                    SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
                    new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            mOnSignInCompleteCallback.run();
                        }

                        @Override
                        public void onSignInAborted() {
                            mediator.switchToTryAgainView();
                        }
                    });
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback) {
            mSigninManager.isAccountManaged(accountInfo, callback);
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public void setUserAcceptedAccountManagement(boolean confirmed) {
            mSigninManager.setUserAcceptedAccountManagement(confirmed);
        }

        /** Implements {@link AccountPickerDelegate}. */
        @Override
        public String extractDomainName(String accountEmail) {
            return mSigninManager.extractDomainName(accountEmail);
        }
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final String mUrl;
    private final String mTitle;
    private final BottomSheetController mController;
    private final Profile mProfile;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    public SendTabToSelfCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            String url,
            String title,
            BottomSheetController controller,
            Profile profile,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mUrl = url;
        mTitle = title;
        mController = controller;
        mProfile = profile;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
    }

    public void show() {
        Optional</*@EntryPointDisplayReason*/ Integer> displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
        assert displayReason.isPresent();

        MetricsRecorder.recordCrossDeviceTabJourney();
        switch (displayReason.get()) {
            case EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE:
                mController.requestShowContent(
                        new NoTargetDeviceBottomSheetContent(mContext, mProfile), true);
                return;
            case EntryPointDisplayReason.OFFER_FEATURE:
                List<TargetDeviceInfo> targetDevices =
                        SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);
                mController.requestShowContent(
                        new DevicePickerBottomSheetContent(
                                mContext, mUrl, mTitle, mController, targetDevices, mProfile),
                        true);
                return;
            case EntryPointDisplayReason.OFFER_SIGN_IN:
                {
                    AccountPickerBottomSheetStrings strings =
                            new AccountPickerBottomSheetStrings.Builder(
                                            R.string
                                                    .signin_account_picker_bottom_sheet_title_for_send_tab_to_self)
                                    .setSubtitleStringId(
                                            R.string
                                                    .signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self)
                                    .setDismissButtonStringId(R.string.cancel)
                                    .build();
                    new AccountPickerBottomSheetCoordinator(
                            mWindowAndroid,
                            mController,
                            new SendTabToSelfAccountPickerDelegate(
                                    this::onSignInComplete,
                                    IdentityServicesProvider.get().getSigninManager(mProfile)),
                            strings,
                            mDeviceLockActivityLauncher,
                            AccountPickerLaunchMode.DEFAULT,
                            /* isWebSignin= */ false,
                            SigninAccessPoint.SEND_TAB_TO_SELF_PROMO);
                    return;
                }
        }
    }

    private void onSignInComplete() {
        new TargetDeviceListWaiter(mController, mUrl, this::onTargetDeviceListReady, mProfile);
    }

    private void onTargetDeviceListReady() {
        mController.hideContent(mController.getCurrentSheetContent(), /* animate= */ true);
        show();
    }
}
