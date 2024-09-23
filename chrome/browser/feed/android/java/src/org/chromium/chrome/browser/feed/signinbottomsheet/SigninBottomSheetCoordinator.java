// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.signinbottomsheet;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetMediator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Coordinator for displaying the signin flow in the bottom sheet. */
public class SigninBottomSheetCoordinator implements AccountPickerDelegate {
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final BottomSheetController mController;
    private final SigninManager mSigninManager;
    private boolean mSetTestToast;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinator;
    private final Runnable mOnSigninSuccessCallback;
    private final AccountPickerBottomSheetStrings mBottomSheetStrings;

    public SigninBottomSheetCoordinator(
            WindowAndroid windowAndroid,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BottomSheetController controller,
            Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @Nullable Runnable onSigninSuccessCallback,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mController = controller;
        mProfile = profile;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mSetTestToast = false;
        mOnSigninSuccessCallback = onSigninSuccessCallback;
        mSigninAccessPoint = signinAccessPoint;
        mBottomSheetStrings = bottomSheetStrings;
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
                "SigninBottomSheetCoordinator.addAccount() should never be called.");
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator) {
        SigninManager.SignInCallback callback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        RecordHistogram.recordBooleanHistogram(
                                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful",
                                true);
                        mController.hideContent(mController.getCurrentSheetContent(), true);
                        if (mOnSigninSuccessCallback != null) {
                            mOnSigninSuccessCallback.run();
                        }
                    }

                    @Override
                    public void onSignInAborted() {
                        RecordHistogram.recordBooleanHistogram(
                                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful",
                                false);
                        mediator.switchToTryAgainView();
                    }
                };

        if (mSigninManager.isSigninAllowed()) {
            mSigninManager.signin(accountInfo, mSigninAccessPoint, callback);
        } else {
            makeSigninNotAllowedToast();
            mController.hideContent(mController.getCurrentSheetContent(), true);
        }
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

    public void show() {
        mAccountPickerBottomSheetCoordinator =
                new AccountPickerBottomSheetCoordinator(
                        mWindowAndroid,
                        mController,
                        this,
                        mBottomSheetStrings,
                        mDeviceLockActivityLauncher,
                        AccountPickerLaunchMode.DEFAULT,
                        /* isWebSignin= */ false,
                        mSigninAccessPoint);
    }

    private void makeSigninNotAllowedToast() {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninDisabledNotificationShown",
                mSigninAccessPoint,
                SigninAccessPoint.MAX);
        if (mSetTestToast) return;
        Toast.makeText(
                        mWindowAndroid.getActivity().get(),
                        R.string.sign_in_to_chrome_disabled_by_user_summary,
                        Toast.LENGTH_SHORT)
                .show();
    }

    public View getBottomSheetViewForTesting() {
        return mAccountPickerBottomSheetCoordinator.getBottomSheetViewForTesting();
    }

    public void setToastOverrideForTesting() {
        this.mSetTestToast = true;
    }
}
