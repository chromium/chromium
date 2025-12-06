// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninAccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;

/** The bridge regroups methods invoked by native code to interact with Android Signin UI. */
@NullMarked
final class SigninBridge {
    /** Used for dependency injection in unit tests. */
    @VisibleForTesting
    static class AccountPickerBottomSheetCoordinatorFactory {
        AccountPickerBottomSheetCoordinator create(
                WindowAndroid windowAndroid,
                IdentityManager identityManager,
                SigninManager signinManager,
                BottomSheetController bottomSheetController,
                AccountPickerDelegate accountPickerDelegate,
                AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
                DeviceLockActivityLauncher deviceLockActivityLauncher,
                @AccountPickerLaunchMode int accountPickerLaunchMode) {
            return new AccountPickerBottomSheetCoordinator(
                    windowAndroid,
                    identityManager,
                    signinManager,
                    bottomSheetController,
                    accountPickerDelegate,
                    accountPickerBottomSheetStrings,
                    deviceLockActivityLauncher,
                    accountPickerLaunchMode,
                    /* isWebSignin= */ true,
                    SigninAccessPoint.WEB_SIGNIN,
                    /* selectedAccountId= */ null);
        }
    }

    @VisibleForTesting static final int ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT = 3;

    /**
     * Starts a flow to add a Google account to the device.
     *
     * @param windowAndroid The window to use for the flow.
     * @param prefilledEmail The email address to prefill in the add account flow, or null if no
     *     email should be prefilled.
     * @param continueUrl The URL to navigate to after the account is added.
     */
    // TODO(crbug.com/432009825): Remove when the redirection will be implemented.
    @SuppressLint("UnusedVariable")
    @CalledByNative
    private static void startAddAccountFlow(
            WindowAndroid windowAndroid,
            @Nullable @JniType("std::string") String prefilledEmail,
            @JniType("GURL") GURL continueUrl) {
        ThreadUtils.assertOnUiThread();
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.createAddAccountIntent(
                prefilledEmail,
                (@Nullable Intent intent) -> {
                    if (intent == null) {
                        // AccountManagerFacade couldn't create intent, use SigninUtils to open
                        // settings instead.
                        Activity activity = windowAndroid.getActivity().get();
                        if (activity != null) {
                            SigninUtils.openSettingsForAllAccounts(activity);
                        }
                        return;
                    }
                    windowAndroid.showIntent(
                            intent,
                            (int resultCode, @Nullable Intent data) -> {
                                // TODO(crbug.com/432009825): Wait for the Cookies to be available,
                                // and navigate to the continue URL.
                            },
                            null);
                });
    }

    /** Opens account management screen. */
    @CalledByNative
    private static void openAccountManagementScreen(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            AccountManagementFragment.openAccountManagementScreen(context, gaiaServiceType);
        }
    }

    /** Opens account picker bottom sheet. */
    @CalledByNative
    private static void openAccountPickerBottomSheet(Tab tab, @JniType("GURL") GURL continueUrl) {
        openAccountPickerBottomSheet(
                tab, continueUrl, new AccountPickerBottomSheetCoordinatorFactory());
    }

    /** Opens account picker bottom sheet. */
    @VisibleForTesting
    static void openAccountPickerBottomSheet(
            Tab tab, GURL continueUrl, AccountPickerBottomSheetCoordinatorFactory factory) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/1145031#c5 and https://crbug.com/323424409 for details.
            return;
        }
        Profile profile = tab.getProfile().getOriginalProfile();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        if (!signinManager.isSigninAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                    SigninAccessPoint.WEB_SIGNIN);
            return;
        }
        List<AccountInfo> accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(
                        AccountManagerFacadeProvider.getInstance().getAccounts());
        if (accounts.isEmpty()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS,
                    SigninAccessPoint.WEB_SIGNIN);
            return;
        }
        if (SigninPreferencesManager.getInstance().getWebSigninAccountPickerActiveDismissalCount()
                >= ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS,
                    SigninAccessPoint.WEB_SIGNIN);
            return;
        }
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            // The bottomSheetController can be null when google.com is just opened inside a
            // bottom sheet for example. In this case, it's better to disable the account picker
            // bottom sheet.
            return;
        }
        final Context context = windowAndroid.getContext().get();
        if (context == null) {
            return;
        }
        // TODO(b/41493784): Update this when the new sign-in flow will be used for the web signin
        // entry point.
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(
                                context.getString(
                                        R.string.signin_account_picker_bottom_sheet_title))
                        .setSubtitleString(
                                context.getString(
                                        R.string
                                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin))
                        .setDismissButtonString(
                                context.getString(R.string.signin_account_picker_dismiss_button))
                        .build();

        factory.create(
                windowAndroid,
                signinManager.getIdentityManager(),
                signinManager,
                bottomSheetController,
                new WebSigninAccountPickerDelegate(tab, new WebSigninBridge.Factory(), continueUrl),
                strings,
                DeviceLockActivityLauncherImpl.get(),
                AccountPickerLaunchMode.DEFAULT);
    }

    private SigninBridge() {}
}
