// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.Optional;

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
        private final String mUrl;
        private final Runnable mGotDeviceListCallback;
        private final Profile mProfile;

        /**
         * Note there's no need for a notion for a failure callback because in that case the
         * account picker bottom sheet was closed and there's nothing left to do (simply don't
         * show any other bottom sheet).
         */
        public TargetDeviceListWaiter(BottomSheetController bottomSheetController, String url,
                Runnable gotDeviceListCallback, Profile profile) {
            mBottomSheetController = bottomSheetController;
            mUrl = url;
            mGotDeviceListCallback = gotDeviceListCallback;
            mProfile = profile;

            SyncServiceFactory.get().addSyncStateChangedListener(this);
            mBottomSheetController.addObserver(this);
            notifyAndDestroyIfDone();
        }

        private void destroy() {
            SyncServiceFactory.get().removeSyncStateChangedListener(this);
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
        private final Profile mProfile;

        public SendTabToSelfAccountPickerDelegate(
                Runnable onSignInCompleteCallback, Profile profile) {
            mOnSignInCompleteCallback = onSignInCompleteCallback;
            mProfile = profile;
        }

        @Override
        public void destroy() {}

        @Override
        public void signIn(
                String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
            Account account = AccountUtils.createAccountFromName(accountEmail);
            signinManager.signin(account, SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
                    new SigninManager.SignInCallback() {
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
        public @EntryPoint int getEntryPoint() {
            return EntryPoint.SEND_TAB_TO_SELF;
        }
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final String mUrl;
    private final String mTitle;
    private final BottomSheetController mController;
    private final Profile mProfile;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    public SendTabToSelfCoordinator(Context context, WindowAndroid windowAndroid, String url,
            String title, BottomSheetController controller, Profile profile,
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
        if (!displayReason.isPresent()) {
            // This must be the old behavior where the entry point is shown even in states where
            // no promo is shown.
            assert !ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO);
            MetricsRecorder.recordSendingEvent(SendingEvent.SHOW_NO_TARGET_DEVICE_MESSAGE);
            mController.requestShowContent(new NoTargetDeviceBottomSheetContent(mContext), true);
            return;
        }

        switch (displayReason.get()) {
            case EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE:
                MetricsRecorder.recordSendingEvent(SendingEvent.SHOW_NO_TARGET_DEVICE_MESSAGE);
                mController.requestShowContent(
                        new NoTargetDeviceBottomSheetContent(mContext), true);
                return;
            case EntryPointDisplayReason.OFFER_FEATURE:
                MetricsRecorder.recordSendingEvent(SendingEvent.SHOW_DEVICE_LIST);
                // TODO(crbug.com/1219434): Merge with INFORM_NO_TARGET_DEVICE, just let the UI
                // differentiate between the 2 by checking the device list size.
                List<TargetDeviceInfo> targetDevices =
                        SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);
                mController.requestShowContent(
                        new DevicePickerBottomSheetContent(
                                mContext, mUrl, mTitle, mController, targetDevices, mProfile),
                        true);
                return;
            case EntryPointDisplayReason.OFFER_SIGN_IN: {
                MetricsRecorder.recordSendingEvent(SendingEvent.SHOW_SIGNIN_PROMO);
                new AccountPickerBottomSheetCoordinator(mWindowAndroid, mController,
                        new SendTabToSelfAccountPickerDelegate(this::onSignInComplete, mProfile),
                        new BottomSheetStrings(), mDeviceLockActivityLauncher);
                return;
            }
        }
    }

    private void onSignInComplete() {
        new TargetDeviceListWaiter(mController, mUrl, this::onTargetDeviceListReady, mProfile);
    }

    private void onTargetDeviceListReady() {
        mController.hideContent(mController.getCurrentSheetContent(), /*animate=*/true);
        show();
    }

    /** A class to store the STTS specific strings for the signin bottom sheet */
    public static class BottomSheetStrings implements AccountPickerBottomSheetStrings {
        /** Returns the title string for the bottom sheet dialog. */
        @Override
        public @StringRes int getTitle() {
            return R.string.signin_account_picker_bottom_sheet_title_for_send_tab_to_self;
        }

        /** Returns the subtitle string for the bottom sheet dialog. */
        @Override
        public @StringRes int getSubtitle() {
            return R.string.signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self;
        }

        /** Returns the cancel button string for the bottom sheet dialog. */
        @Override
        public @StringRes int getDismissButton() {
            return R.string.cancel;
        }
    }
}
