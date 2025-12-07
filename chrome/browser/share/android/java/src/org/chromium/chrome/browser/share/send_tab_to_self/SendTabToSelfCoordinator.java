// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
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

/** Coordinator for displaying the send tab to self feature. */
@NullMarked
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

            assumeNonNull(SyncServiceFactory.getForProfile(mProfile))
                    .addSyncStateChangedListener(this);
            mBottomSheetController.addObserver(this);
            notifyAndDestroyIfDone();
        }

        private void destroy() {
            assumeNonNull(SyncServiceFactory.getForProfile(mProfile))
                    .removeSyncStateChangedListener(this);
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
            @EntryPointDisplayReason
            Integer displayReason =
                    SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
            // The model is starting up, keep waiting.
            if (displayReason == null) return;

            switch (displayReason) {
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

        public SendTabToSelfAccountPickerDelegate(Runnable onSignInCompleteCallback) {
            mOnSignInCompleteCallback = onSignInCompleteCallback;
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
        public void onSignInComplete(
                CoreAccountInfo accountInfo,
                AccountPickerDelegate.SigninStateController controller) {
            controller.onSigninComplete();
            mOnSignInCompleteCallback.run();
        }
    }

    private final Context mContext;
    private final @Nullable WindowAndroid mWindowAndroid;
    private final String mUrl;
    private final String mTitle;
    private final BottomSheetController mController;
    private final Profile mProfile;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    public SendTabToSelfCoordinator(
            Context context,
            @Nullable WindowAndroid windowAndroid,
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
        @EntryPointDisplayReason
        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
        assert displayReason != null;

        MetricsRecorder.recordCrossDeviceTabJourney();
        switch (displayReason) {
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
                                            mContext.getString(
                                                    R.string
                                                            .signin_account_picker_bottom_sheet_title_for_send_tab_to_self))
                                    .setSubtitleString(
                                            mContext.getString(
                                                    R.string
                                                            .signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self))
                                    .setDismissButtonString(mContext.getString(R.string.cancel))
                                    .build();
                    var identityManager =
                            IdentityServicesProvider.get().getIdentityManager(mProfile);
                    var signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
                    new AccountPickerBottomSheetCoordinator(
                            assertNonNull(mWindowAndroid),
                            assertNonNull(identityManager),
                            assertNonNull(signinManager),
                            mController,
                            new SendTabToSelfAccountPickerDelegate(this::onSignInComplete),
                            strings,
                            mDeviceLockActivityLauncher,
                            AccountPickerLaunchMode.DEFAULT,
                            /* isWebSignin= */ false,
                            SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
                            /* selectedAccountId= */ null);
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
