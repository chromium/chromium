// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.util.HashUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/**
 * Class that drives the account mismatch notification flow. Works in conjunction with {@link
 * BrandingController} for global branding rate-limiting policy.
 */
@NullMarked
public class MismatchNotificationChecker
        implements MismatchNotificationSigninDelegate,
                BottomSheetSigninAndHistorySyncCoordinator.Delegate {
    private final Context mContext;
    private final Profile mProfile;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private final SigninAndHistorySyncActivityLauncher mSigninLauncher;
    private final CallbackController mCallbackController = new CallbackController();

    /** Used to suppress IPH UIs while the mismatch notification UI is on the screen. */
    private Tracker.@Nullable DisplayLockHandle mFeatureEngagementLock;

    /**
     * Whether the other prompt UIs should be suppressed. This var is set to {@code true} while the
     * mismatch notification UI is on the screen.
     */
    private boolean mShouldSuppressPromptUis;

    // TODO(crbug.com/448227402): Removing nullability after migration
    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;

    /** Interface bridging the checker with the account mismatch rate-limiting logic. */
    public interface Delegate {
        /**
         * Show the account mismatch UI if conditions are right.
         *
         * @param signinDelegate The delegate for the controller to be used for the sign-in flow.
         * @param accountId Account ID to be used to access notification data.
         * @param lastShownTime The last time the notification was shown to user.
         * @param mimData Account mismatch notification data.
         * @param onClose Callback to be invoked when the notification is closed.
         * @return Whether the UI will be shown or not.
         */
        boolean maybeShow(
                MismatchNotificationSigninDelegate signinDelegate,
                String accountId,
                long lastShownTime,
                @Nullable MismatchNotificationData mimData,
                Callback<Integer> onClose);
    }

    /**
     * Constructor.
     *
     * @param activity The hosting activity, used by sign-in launcher to anchor the bottomsheet.
     * @param windowAndroid The window android.
     * @param activityResultTracker The activity result tracker.
     * @param deviceLockActivityLauncher The device lock activity launcher.
     * @param profile The current profile object.
     * @param identityManager The manager providing the account info.
     * @param signinLauncher The launcher for sign-in activity.
     * @param bottomSheetControllerSupplier The bottom sheet controller supplier.
     * @param modalDialogManager The modal dialog manager.
     * @param snackbarManager The snackbar manager.
     * @param delegate Delegate providing the actual decision/UI logic.
     */
    public MismatchNotificationChecker(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            Profile profile,
            IdentityManager identityManager,
            SigninAndHistorySyncActivityLauncher signinLauncher,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            Delegate delegate) {
        mContext = activity;
        mProfile = profile;
        mIdentityManager = identityManager;
        mSigninLauncher = signinLauncher;
        mDelegate = delegate;

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
            profileSupplier.set(profile);
            mSigninCoordinator =
                    mSigninLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                            windowAndroid,
                            activity,
                            activityResultTracker,
                            this,
                            deviceLockActivityLauncher,
                            profileSupplier,
                            bottomSheetControllerSupplier,
                            modalDialogManager,
                            snackbarManager,
                            SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION);
        }
    }

    /** Show account mismatch notification UI if all the conditions are met. */
    public boolean maybeShow(
            String appId,
            long lastShowTime,
            @Nullable MismatchNotificationData data,
            Callback<MismatchNotificationData> closeCallback) {
        String accountId = getAccountId();
        MismatchNotificationData mimData = data; // effective final
        boolean show =
                mDelegate.maybeShow(
                        this,
                        accountId,
                        lastShowTime,
                        data,
                        mCallbackController.makeCancelable(
                                (closeType) -> {
                                    mShouldSuppressPromptUis = false;
                                    if (mFeatureEngagementLock != null) {
                                        mFeatureEngagementLock.release();
                                        mFeatureEngagementLock = null;
                                    }
                                    // The UI was not visible. Do not do the update.
                                    if (closeType == CloseType.UNKNOWN.getNumber()) return;

                                    MismatchNotificationData res =
                                            mimData != null
                                                    ? mimData
                                                    : new MismatchNotificationData();
                                    var appData = res.getAppData(accountId, appId);
                                    appData.showCount++;
                                    appData.closeType = closeType;
                                    if (closeType == CloseType.DISMISSED.getNumber()
                                            || closeType == CloseType.ACCEPTED.getNumber()) {
                                        appData.userActCount++;
                                    }
                                    res.setAppData(accountId, appId, appData);
                                    closeCallback.onResult(res);
                                }));
        if (show) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
            mFeatureEngagementLock = tracker.acquireDisplayLock();
            mShouldSuppressPromptUis = true;
        }
        return show;
    }

    /** Implements {@link MismatchNotificationControllerDelegate}. */
    @Override
    public void startSignin(BottomSheetSigninAndHistorySyncConfig config) {
        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            assert mSigninCoordinator != null;
            mSigninCoordinator.startSigninFlow(config);
        } else {
            // Fallback activity flow
            @Nullable Intent intent =
                    mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                            mContext,
                            mProfile,
                            config,
                            SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION);
            if (intent != null) {
                mContext.startActivity(intent);
            }
        }
    }

    /** Returns a cropped hash of the currently sign-in account ID. */
    @VisibleForTesting
    String getAccountId() {
        @Nullable AccountInfo account = mIdentityManager.getPrimaryAccountInfo();
        if (account == null) return "";
        var hash = HashUtil.getMd5Hash(new HashUtil.Params(account.getGaiaId().toString()));
        if (hash == null) return "";
        return hash.substring(0, 16);
    }

    /** Whether prompt Ui components should be temporaily suppressed. */
    public boolean shouldSuppressPromptUis() {
        return mShouldSuppressPromptUis;
    }

    public void destroy() {
        mCallbackController.destroy();
        mShouldSuppressPromptUis = false;
        if (mFeatureEngagementLock != null) {
            mFeatureEngagementLock.release();
            mFeatureEngagementLock = null;
        }
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }
    }
}
