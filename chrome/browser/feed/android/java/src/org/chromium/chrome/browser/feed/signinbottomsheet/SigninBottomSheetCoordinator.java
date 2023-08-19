// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.signinbottomsheet;

import android.accounts.Account;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
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

    public SigninBottomSheetCoordinator(WindowAndroid windowAndroid,
            DeviceLockActivityLauncher deviceLockActivityLauncher, BottomSheetController controller,
            Profile profile, @Nullable AccountPickerBottomSheetStrings bottomSheetStrings,
            @Nullable Runnable onSigninSuccessCallback, @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mController = controller;
        mProfile = profile;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mSetTestToast = false;
        mOnSigninSuccessCallback = onSigninSuccessCallback;
        mSigninAccessPoint = signinAccessPoint;
        mBottomSheetStrings =
                bottomSheetStrings != null ? bottomSheetStrings : new BottomSheetStrings();
    }

    @Override
    public void destroy() {}

    @Override
    public void signIn(
            String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        Account account = AccountUtils.createAccountFromName(accountEmail);
        SigninManager.SignInCallback callback = new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", true);
                mController.hideContent(mController.getCurrentSheetContent(), true);
                if (mOnSigninSuccessCallback != null) {
                    mOnSigninSuccessCallback.run();
                }
            }

            @Override
            public void onSignInAborted() {
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", false);
                // onSignInErrorCallback is called by the WebSigninBridge which is
                // not implemented in this signin flow as we do not need to wait for
                // cookies to propagate before proceeding with the Feed refresh.
                // Instead of calling
                // AccountPickerBottomSheetMediator.onSigninFailed() from the signin
                // bridge we directly perform the creation of the "try again" bottom
                // sheet view:
                mAccountPickerBottomSheetCoordinator.setTryAgainBottomSheetView();
            }
        };

        AccountInfoServiceProvider.get().getAccountInfoByEmail(accountEmail).then(accountInfo -> {
            if (mSigninManager.isSigninAllowed()) {
                mSigninManager.signin(account, mSigninAccessPoint, callback);
            } else {
                makeSigninNotAllowedToast();
                mController.hideContent(mController.getCurrentSheetContent(), true);
            }
        });
    }

    @Override
    public @EntryPoint int getEntryPoint() {
        return EntryPoint.FEED_ACTION;
    }

    public void show() {
        mAccountPickerBottomSheetCoordinator =
                new AccountPickerBottomSheetCoordinator(mWindowAndroid, mController, this,
                        mBottomSheetStrings, mDeviceLockActivityLauncher);
    }

    private void makeSigninNotAllowedToast() {
        RecordHistogram.recordEnumeratedHistogram("Signin.SigninDisabledNotificationShown",
                mSigninAccessPoint, SigninAccessPoint.MAX);
        if (mSetTestToast) return;
        Toast.makeText(mWindowAndroid.getActivity().get(),
                     R.string.sign_in_to_chrome_disabled_by_user_summary, Toast.LENGTH_SHORT)
                .show();
    }

    public View getBottomSheetViewForTesting() {
        return mAccountPickerBottomSheetCoordinator.getBottomSheetViewForTesting();
    }

    @VisibleForTesting
    public void setAccountPickerBottomSheetCoordinator(
            AccountPickerBottomSheetCoordinator accountPickerBottomSheetCoordinator) {
        this.mAccountPickerBottomSheetCoordinator = accountPickerBottomSheetCoordinator;
    }

    public void setToastOverrideForTesting() {
        this.mSetTestToast = true;
    }

    /** Stores bottom sheet strings for signin from back of card entry point */
    public static class BottomSheetStrings implements AccountPickerBottomSheetStrings {
        /** Returns the title string for the bottom sheet dialog. */
        @Override
        public @StringRes int getTitle() {
            return R.string.signin_account_picker_bottom_sheet_title_for_back_of_card_menu_signin;
        }

        /** Returns the subtitle string for the bottom sheet dialog. */
        @Override
        public @StringRes int getSubtitle() {
            return R.string
                    .signin_account_picker_bottom_sheet_subtitle_for_back_of_card_menu_signin;
        }

        /** Returns the cancel button string for the bottom sheet dialog. */
        @Override
        public @StringRes int getDismissButton() {
            return R.string.close;
        }
    }
}
