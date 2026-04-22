// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.accounts.AccountManager;
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
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.WebSigninAndHistorySyncCoordinatorSupplier;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninAccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninDelegateContext;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
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
                @AccountPickerLaunchMode int accountPickerLaunchMode,
                @Nullable CoreAccountId selectedAccountId) {
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
                    selectedAccountId);
        }
    }

    @VisibleForTesting static final int ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT = 3;

    /**
     * Starts a flow to add a Google account to the device. A bottomsheet will be opened after there
     * is no primary account.
     *
     * @param tab The target tab for the continueUrl navigation.
     * @param prefilledEmail The email address to prefill in the add account flow, or null if no
     *     email should be prefilled.
     * @param continueUrl The URL to navigate to after the account is added.
     */
    @CalledByNative
    private static void startAddAccountFlow(
            Tab tab,
            @Nullable @JniType("std::string") String prefilledEmail,
            @JniType("GURL") GURL continueUrl) {
        startAddAccountFlow(
                tab, prefilledEmail, continueUrl, new AccountPickerBottomSheetCoordinatorFactory());
    }

    /** See {@link SigninBridge#startAddAccountFlow()} above. */
    @VisibleForTesting
    static void startAddAccountFlow(
            Tab tab,
            @Nullable String prefilledEmail,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/40729225#c5 and https://crbug.com/323424409 for details.
            return;
        }
        GURL initialTabURL = tab.getUrl();
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
                            getIntentCallback(
                                    tab, prefilledEmail, continueUrl, factory, initialTabURL),
                            null);
                });
    }

    private static WindowAndroid.IntentCallback getIntentCallback(
            Tab tab,
            @Nullable String prefilledEmail,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory,
            GURL initialTabURL) {
        return (int resultCode, @Nullable Intent data) -> {
            @Nullable String addedAccountEmail =
                    data == null
                            ? prefilledEmail
                            : data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_ADD_SESSION_REDIRECT)
                    && resultCode == Activity.RESULT_OK) {
                IdentityManager identityManager =
                        assumeNonNull(
                                IdentityServicesProvider.get()
                                        .getIdentityManager(tab.getProfile().getOriginalProfile()));

                // If the account is added to the device but there is no primary
                // account then surface the bottom sheet otherwise wait for
                // cookies to be minted.
                if (identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null) {
                    openAccountPickerBottomSheet(
                            tab,
                            continueUrl,
                            factory,
                            assumeNonNull(
                                            identityManager.findExtendedAccountInfoByEmailAddress(
                                                    assumeNonNull(addedAccountEmail)))
                                    .getId());
                    return;
                }

                waitForCookiesAndRedirect(tab, addedAccountEmail, continueUrl, initialTabURL);
            }
        };
    }

    /**
     * Redirects to the continueUrl in the given tab if refresh tokens and cookies are minted for
     * the account associated with the prefilledEmail.
     */
    private static void waitForCookiesAndRedirect(
            Tab tab, @Nullable String prefilledEmail, GURL continueUrl, GURL initialTabURL) {
        assert prefilledEmail != null;
        @Nullable WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return;
        WebSigninRedirectCoordinator coordinator =
                WebSigninRedirectCoordinatorSupplier.getOrCreateCoordinatorFrom(windowAndroid);
        coordinator.initializeWebSigninAndRedirect(tab, prefilledEmail, continueUrl, initialTabURL);
    }

    /**
     * Redirects to the continueUrl in the given tab if refresh tokens and cookies are minted for
     * the account associated with the selectedAccountId.
     */
    @CalledByNative
    public static void waitForCookiesAndRedirect(
            Tab tab,
            @JniType("GURL") GURL continueUrl,
            @JniType("std::optional<CoreAccountId>") CoreAccountId selectedAccountId) {
        waitForCookiesAndRedirect(tab, selectedAccountId, continueUrl, tab.getUrl());
    }

    /**
     * Redirects to the continueUrl in the given tab if refresh tokens and cookies are minted for
     * the account associated with the selectedAccountId.
     */
    private static void waitForCookiesAndRedirect(
            Tab tab, CoreAccountId selectedAccountId, GURL continueUrl, GURL initialTabURL) {
        @Nullable WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return;
        WebSigninRedirectCoordinator coordinator =
                WebSigninRedirectCoordinatorSupplier.getOrCreateCoordinatorFrom(windowAndroid);
        coordinator.initializeWebSigninAndRedirect(
                tab, selectedAccountId, continueUrl, initialTabURL);
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
    private static void openAccountPickerBottomSheet(
            Tab tab,
            @JniType("GURL") GURL continueUrl,
            @Nullable @JniType("std::optional<CoreAccountId>") CoreAccountId selectedAccountId) {
        openAccountPickerBottomSheet(
                tab,
                continueUrl,
                new AccountPickerBottomSheetCoordinatorFactory(),
                selectedAccountId);
    }

    /** Opens account picker bottom sheet. */
    @VisibleForTesting
    static void openAccountPickerBottomSheet(
            Tab tab,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory,
            @Nullable CoreAccountId selectedAccountId) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/40729225#c5 and https://crbug.com/323424409 for details.
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

        // If the web requests a sign-in with a specific account that is present on the device the
        // impression limit is ignored.
        if (selectedAccountId == null
                && SigninPreferencesManager.getInstance()
                                .getWebSigninAccountPickerActiveDismissalCount()
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

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            BottomSheetSigninAndHistorySyncConfig.Builder builder =
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                            strings,
                            NoAccountSigninMode.BOTTOM_SHEET,
                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                            HistorySyncConfig.OptInMode.NONE,
                            context.getString(R.string.history_sync_title),
                            context.getString(R.string.history_sync_subtitle));
            if (selectedAccountId != null) {
                builder.selectedCoreAccountId(selectedAccountId);
            }
            BottomSheetSigninAndHistorySyncConfig config = builder.build();
            BottomSheetSigninAndHistorySyncCoordinator coordinator =
                    assertNonNull(
                            WebSigninAndHistorySyncCoordinatorSupplier.getValueOrNullFrom(
                                    windowAndroid));
            coordinator.startSigninFlow(
                    config, new WebSigninDelegateContext(tab.getId(), continueUrl));
            return;
        }

        factory.create(
                windowAndroid,
                signinManager.getIdentityManager(),
                signinManager,
                bottomSheetController,
                new WebSigninAccountPickerDelegate(tab, new WebSigninBridge.Factory(), continueUrl),
                strings,
                DeviceLockActivityLauncherImpl.get(),
                AccountPickerLaunchMode.DEFAULT,
                selectedAccountId);
    }

    /**
     * Starts the flow to reauthenticate.
     *
     * @param tab The target tab for the continueUrl navigation.
     * @param continueUrl The URL to navigate to after the reauthentication. This will not be an
     *     empty string.
     * @param selectedAccountId The account to be reauthenticated .
     */
    @CalledByNative
    private static void startUpdateCredentialsFlow(
            Tab tab,
            @JniType("GURL") GURL continueUrl,
            @JniType("CoreAccountId") CoreAccountId selectedAccountId) {
        assert selectedAccountId != null;
        assert continueUrl != null;
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/40729225#c5 and https://crbug.com/323424409 for details.
            return;
        }
        GURL initialTabURL = tab.getUrl();
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        Profile profile = tab.getProfile().getOriginalProfile();
        IdentityManager identityManager =
                assertNonNull(IdentityServicesProvider.get().getIdentityManager(profile));

        accountManagerFacade.updateCredentials(
                assertNonNull(
                        identityManager.findExtendedAccountInfoByAccountId(selectedAccountId)),
                assumeNonNull(windowAndroid.getActivity().get()),
                (success) -> {
                    if (success && !tab.isDestroyed() && tab.getUrl().equals(initialTabURL)) {
                        waitForCookiesAndRedirect(
                                tab, selectedAccountId, continueUrl, initialTabURL);
                    }
                });
    }

    private SigninBridge() {}
}
