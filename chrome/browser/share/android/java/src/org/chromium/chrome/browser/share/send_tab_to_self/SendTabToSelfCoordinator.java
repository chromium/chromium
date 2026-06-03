// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.PostSigninOperationResult;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/** Coordinator for displaying the send tab to self feature. */
@NullMarked
public class SendTabToSelfCoordinator
        implements BottomSheetSigninAndHistorySyncCoordinator.Delegate {

    /**
     * Waits for Sync to download the list of target devices after sign-in. Aborts if the user
     * dismisses the sign-in bottom sheet ("account picker") before success.
     */
    // TODO(crbug.com/519101926): Consider moving TargetDeviceListWaiter to a shared C++
    // component (components/send_tab_to_self) and accessing it via JNI to reduce duplication.
    private static class TargetDeviceListWaiter extends EmptyBottomSheetObserver
            implements SyncService.SyncStateChangedListener {
        private final BottomSheetController mBottomSheetController;
        private final String mUrl;
        private final Callback<@PostSigninOperationResult Integer> mOnComplete;
        private final Profile mProfile;

        /**
         * Note there's no need for a notion for a failure callback because in that case the account
         * picker bottom sheet was closed and there's nothing left to do (simply don't show any
         * other bottom sheet).
         */
        public TargetDeviceListWaiter(
                BottomSheetController bottomSheetController,
                String url,
                Callback<@PostSigninOperationResult Integer> onComplete,
                Profile profile) {
            mBottomSheetController = bottomSheetController;
            mUrl = url;
            mOnComplete = onComplete;
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
            mOnComplete.onResult(PostSigninOperationResult.SUCCESS);
        }
    }

    /**
     * Performs sign-in for the promo shown to signed-out users. TODO(crbug.com/448227402): Remove
     * this class after migration to the activity-less sign-in flow is complete.
     */
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
    private final BottomSheetController mBottomSheetController;
    private final Profile mProfile;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final Supplier<@Nullable Tab> mTabProvider;
    private final Activity mActivity;
    private final SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private final ActivityResultTracker mActivityResultTracker;
    private final MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final SnackbarManager mSnackbarManager;
    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;

    public SendTabToSelfCoordinator(
            Context context,
            @Nullable WindowAndroid windowAndroid,
            String url,
            String title,
            BottomSheetController bottomSheetController,
            Profile profile,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            Supplier<@Nullable Tab> tabProvider,
            Activity activity,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            ActivityResultTracker activityResultTracker,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mUrl = url;
        mTitle = title;
        mBottomSheetController = bottomSheetController;
        mProfile = profile;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mTabProvider = tabProvider;
        mActivity = activity;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        mActivityResultTracker = activityResultTracker;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManager = snackbarManager;
    }

    public void show() {
        @EntryPointDisplayReason
        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
        assert displayReason != null;

        SendTabToSelfMetricsRecorder.recordCrossDeviceTabJourney();
        switch (displayReason) {
            case EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE:
                mBottomSheetController.requestShowContent(
                        new NoTargetDeviceBottomSheetContent(mContext, mProfile), true);
                return;
            case EntryPointDisplayReason.OFFER_FEATURE:
                List<TargetDeviceInfo> targetDevices =
                        SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);
                mBottomSheetController.requestShowContent(
                        new DevicePickerBottomSheetContent(
                                mContext,
                                mUrl,
                                mTitle,
                                mBottomSheetController,
                                targetDevices,
                                mProfile,
                                mTabProvider),
                        true);
                return;
            case EntryPointDisplayReason.OFFER_SIGN_IN:
                {
                    AccountPickerBottomSheetStrings bottomSheetStrings =
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
                    if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
                        OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
                        profileSupplier.set(mProfile);
                        SupplierUtils.waitForAll(
                                () -> {
                                    // The BottomSheetSigninAndHistorySyncCoordinator is created on
                                    // demand when the user taps on the Send Tab to Self option,
                                    // rather than when the activity is created. This means that if
                                    // the Chrome Activity is killed during the "Add account" flow,
                                    // the sign-in process will not resume after the account is
                                    // added. This is a known limitation and differs from other
                                    // sign-in entry points due to the complexity of the Send Tab to
                                    // Self feature. This implementation may be subject to change in
                                    // the future.
                                    mSigninCoordinator =
                                            mSigninAndHistorySyncActivityLauncher
                                                    .createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                                            assertNonNull(mWindowAndroid),
                                                            mActivity,
                                                            mActivityResultTracker,
                                                            this,
                                                            mDeviceLockActivityLauncher,
                                                            profileSupplier,
                                                            () -> mBottomSheetController,
                                                            SupplierUtils.asNonNull(
                                                                            mModalDialogManagerSupplier)
                                                                    .get(),
                                                            mSnackbarManager,
                                                            SigninAccessPoint
                                                                    .SEND_TAB_TO_SELF_PROMO);
                                    BottomSheetSigninAndHistorySyncConfig config =
                                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                                            bottomSheetStrings,
                                                            NoAccountSigninMode.BOTTOM_SHEET,
                                                            WithAccountSigninMode
                                                                    .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                                            HistorySyncConfig.OptInMode.NONE,
                                                            mContext.getString(
                                                                    R.string.history_sync_title),
                                                            mContext.getString(
                                                                    R.string.history_sync_subtitle))
                                                    .build();
                                    assert mSigninCoordinator != null;
                                    mSigninCoordinator.startSigninFlow(config);
                                },
                                mModalDialogManagerSupplier);
                    } else {
                        var identityManager =
                                IdentityServicesProvider.get().getIdentityManager(mProfile);
                        var signinManager =
                                IdentityServicesProvider.get().getSigninManager(mProfile);
                        new AccountPickerBottomSheetCoordinator(
                                assertNonNull(mWindowAndroid),
                                assertNonNull(identityManager),
                                assertNonNull(signinManager),
                                mBottomSheetController,
                                new SendTabToSelfAccountPickerDelegate(this::onSignInComplete),
                                bottomSheetStrings,
                                mDeviceLockActivityLauncher,
                                AccountPickerLaunchMode.DEFAULT,
                                /* isWebSignin= */ false,
                                SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
                                /* selectedAccountId= */ null);
                    }
                    return;
                }
        }
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void runPostSigninAction(
            CoreAccountInfo signedInAccount,
            @Nullable DelegateContext delegateContext,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        assert SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled();
        new TargetDeviceListWaiter(mBottomSheetController, mUrl, onComplete, mProfile);
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void onFlowComplete(SigninAndHistorySyncCoordinator.Result result) {
        assert SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled();
        if (result.hasSignedIn) {
            show();
        }
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }
    }

    private void onSignInComplete() {
        assert !SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled();
        new TargetDeviceListWaiter(
                mBottomSheetController, mUrl, (result) -> onTargetDeviceListReady(), mProfile);
    }

    private void onTargetDeviceListReady() {
        mBottomSheetController.hideContent(
                mBottomSheetController.getCurrentSheetContent(), /* animate= */ true);
        show();
    }
}
