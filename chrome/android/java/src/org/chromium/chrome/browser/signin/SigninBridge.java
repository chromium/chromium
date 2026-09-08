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
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
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
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinatorSupplier;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinatorSupplier.SupplierFlow;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.SigninDelegateContext;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninAccountPickerDelegate;
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
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.ExternalEntryPoint;
import org.chromium.components.signin.base.SigninDeepLinkPayload;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.CrossDeviceInitialState;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.List;

/** The bridge regroups methods invoked by native code to interact with Android Signin UI. */
@NullMarked
final class SigninBridge {

    private static final String TAG = "SigninBridge";

    /** Used for dependency injection in unit tests. */
    @VisibleForTesting
    static class AccountPickerBottomSheetCoordinatorFactory {
        AccountPickerBottomSheetCoordinator create(
                WindowAndroid windowAndroid,
                IdentityManager identityManager,
                SigninManager signinManager,
                @Nullable AccountPreviewDataService accountPreviewDataService,
                ModalDialogManager modalDialogManager,
                BottomSheetController bottomSheetController,
                AccountPickerDelegate accountPickerDelegate,
                AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
                DeviceLockActivityLauncher deviceLockActivityLauncher,
                @AccountPickerLaunchMode int accountPickerLaunchMode,
                boolean isWebSignin,
                @SigninAccessPoint int accessPoint,
                @Nullable CoreAccountId selectedAccountId) {
            return new AccountPickerBottomSheetCoordinator(
                    windowAndroid,
                    identityManager,
                    signinManager,
                    accountPreviewDataService,
                    modalDialogManager,
                    bottomSheetController,
                    accountPickerDelegate,
                    accountPickerBottomSheetStrings,
                    deviceLockActivityLauncher,
                    accountPickerLaunchMode,
                    isWebSignin,
                    accessPoint,
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
     * @param extensionName The name of the extension requesting the account. Can be empty if the
     *     flow is not started by an extension.
     */
    @CalledByNative
    private static void startAddAccountFlow(
            Tab tab,
            @Nullable @JniType("std::string") String prefilledEmail,
            @JniType("GURL") GURL continueUrl,
            @JniType("std::string") String extensionName) {
        startAddAccountFlow(
                tab,
                prefilledEmail,
                continueUrl,
                new AccountPickerBottomSheetCoordinatorFactory(),
                extensionName);
    }

    /** See {@link SigninBridge#startAddAccountFlow()} above. */
    @VisibleForTesting
    static void startAddAccountFlow(
            Tab tab,
            @Nullable String prefilledEmail,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory,
            String extensionName) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/40729225#comment6 and https://crbug.com/323424409 for details.
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
                                    tab,
                                    prefilledEmail,
                                    continueUrl,
                                    factory,
                                    initialTabURL,
                                    extensionName),
                            null);
                });
    }

    private static WindowAndroid.IntentCallback getIntentCallback(
            Tab tab,
            @Nullable String prefilledEmail,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory,
            GURL initialTabURL,
            String extensionName) {
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
                if (identityManager.getPrimaryAccountInfo() == null) {
                    openAccountPickerBottomSheet(
                            tab,
                            continueUrl,
                            factory,
                            assumeNonNull(
                                            identityManager.findExtendedAccountInfoByEmailAddress(
                                                    assumeNonNull(addedAccountEmail)))
                                    .getId(),
                            extensionName);
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
    static void openAccountManagementScreen(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/8225307): Allowlist DeviceInfo.isDesktop() for this use case or branch
        // in native code for desktop.
        if (DeviceInfo.isDesktop()
                && SigninFeatureMap.isEnabled(
                        SigninFeatures.OPEN_SYSTEM_ACCOUNT_SETTINGS_DIRECTLY)) {
            Activity activity = windowAndroid.getActivity().get();
            if (activity != null) {
                SigninUtils.openSettingsForAllAccounts(activity);
            }
            return;
        }
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            AccountManagementFragment.openAccountManagementScreen(context, gaiaServiceType);
        }
    }

    /** Opens account picker bottom sheet. */
    @CalledByNative
    private static void openAccountPickerBottomSheetForWebSignin(
            Tab tab,
            @JniType("GURL") GURL continueUrl,
            @Nullable @JniType("std::optional<CoreAccountId>") CoreAccountId selectedAccountId) {
        openAccountPickerBottomSheet(
                tab,
                continueUrl,
                new AccountPickerBottomSheetCoordinatorFactory(),
                selectedAccountId,
                /* extensionName= */ "");
    }

    /** Opens account picker bottom sheet for the extensions API. */
    @CalledByNative
    private static void openAccountPickerBottomSheetForExtensions(
            Tab tab,
            @JniType("GURL") GURL continueUrl,
            @Nullable @JniType("std::optional<CoreAccountId>") CoreAccountId selectedAccountId,
            @JniType("std::string") String extensionName) {
        openAccountPickerBottomSheet(
                tab,
                continueUrl,
                new AccountPickerBottomSheetCoordinatorFactory(),
                selectedAccountId,
                assertNonNull(extensionName));
    }

    /** Opens account picker bottom sheet. */
    @VisibleForTesting
    static void openAccountPickerBottomSheet(
            Tab tab,
            GURL continueUrl,
            AccountPickerBottomSheetCoordinatorFactory factory,
            @Nullable CoreAccountId selectedAccountId,
            String extensionName) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/40729225#comment6 and https://crbug.com/323424409 for details.
            return;
        }
        Profile profile = tab.getProfile().getOriginalProfile();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);

        // The signin bridge bottom sheet is only used for web signin or extensions.
        final @SigninAccessPoint int accessPoint =
                extensionName.isEmpty()
                        ? SigninAccessPoint.WEB_SIGNIN
                        : SigninAccessPoint.EXTENSIONS;
        if (!signinManager.isSigninAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED, accessPoint);
            return;
        }
        List<AccountInfo> accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(
                        AccountManagerFacadeProvider.getInstance().getAccounts());
        if (accounts.isEmpty()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS, accessPoint);
            return;
        }

        final boolean isWebSignin = extensionName.isEmpty();
        // If the web requests a sign-in with a specific account that is present on the device the
        // impression limit is ignored.
        if (isWebSignin
                && selectedAccountId == null
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
                buildAccountPickerBottomSheetStrings(context, extensionName);

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
                            BottomSheetSigninAndHistorySyncCoordinatorSupplier.getValueForFlow(
                                    windowAndroid,
                                    isWebSignin
                                            ? SupplierFlow.WEB_SIGNIN
                                            : SupplierFlow.EXTENSIONS));
            if (isWebSignin) {
                coordinator.startSigninFlow(
                        config, new SigninDelegateContext(tab.getId(), continueUrl));
            } else {
                coordinator.startSigninFlow(config);
            }
            return;
        }

        AccountPreviewDataService accountPreviewDataService =
                IdentityServicesProvider.get().getAccountPreviewDataService(profile);
        factory.create(
                windowAndroid,
                signinManager.getIdentityManager(),
                signinManager,
                accountPreviewDataService,
                assertNonNull(windowAndroid.getModalDialogManager()),
                bottomSheetController,
                new WebSigninAccountPickerDelegate(tab, new WebSigninBridge.Factory(), continueUrl),
                strings,
                DeviceLockActivityLauncherImpl.get(),
                AccountPickerLaunchMode.DEFAULT,
                /* isWebSignin= */ true,
                SigninAccessPoint.WEB_SIGNIN,
                selectedAccountId);
    }

    private static AccountPickerBottomSheetStrings buildAccountPickerBottomSheetStrings(
            Context context, String extensionName) {
        String titleString =
                extensionName.isEmpty()
                        ? context.getString(R.string.signin_account_picker_bottom_sheet_title)
                        : context.getString(
                                R.string.signin_account_picker_bottom_sheet_title_for_extensions,
                                assertNonNull(extensionName));
        String subtitleString =
                extensionName.isEmpty()
                        ? context.getString(
                                R.string.signin_account_picker_bottom_sheet_subtitle_for_web_signin)
                        : context.getString(
                                R.string
                                        .signin_account_picker_bottom_sheet_subtitle_for_extensions);
        return new AccountPickerBottomSheetStrings.Builder(titleString)
                .setSubtitleString(subtitleString)
                .setDismissButtonString(
                        context.getString(R.string.signin_account_picker_dismiss_button))
                .build();
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
            // https://crbug.com/40729225#comment6 and https://crbug.com/323424409 for details.
            return;
        }
        GURL initialTabURL = tab.getUrl();
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.updateCredentials(
                selectedAccountId,
                assumeNonNull(windowAndroid.getActivity().get()),
                (success) -> {
                    if (success && !tab.isDestroyed() && tab.getUrl().equals(initialTabURL)) {
                        waitForCookiesAndRedirect(
                                tab, selectedAccountId, continueUrl, initialTabURL);
                    }
                });
    }

    /**
     * Start the deep link sign-in flow based on the given payload.
     *
     * @param windowAndroid The window where the flow was initiated.
     * @param profile The profile where the flow was initiated.
     * @param payload The deep link payload.
     */
    @CalledByNative
    static void startSigninDeepLinkFlow(
            WindowAndroid windowAndroid,
            Profile profile,
            @JniType("signin::SigninDeepLinkPayload") SigninDeepLinkPayload payload) {
        @Nullable Context context = windowAndroid.getContext().get();
        @Nullable IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (context == null || identityManager == null) {
            return;
        }
        startSigninDeepLinkFlow(context, profile, identityManager, payload);
    }

    private static void startSigninDeepLinkFlow(
            Context context,
            Profile profile,
            IdentityManager identityManager,
            SigninDeepLinkPayload payload) {
        ThreadUtils.assertOnUiThread();

        final @Nullable CoreAccountInfo primaryAccountInfo =
                identityManager.getPrimaryAccountInfo();

        final @Nullable AccountInfo targetAccountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(payload.getEmail());

        boolean isTargetAccountSignedIn =
                primaryAccountInfo != null
                        && targetAccountInfo != null
                        && primaryAccountInfo.getId().equals(targetAccountInfo.getId());

        if (isTargetAccountSignedIn) {
            SigninMetricsUtils.recordCrossDeviceInitialState(
                    payload.getExternalEntryPoint(),
                    CrossDeviceInitialState.SIGNED_IN_WITH_TARGET_ACCOUNT);
            String message =
                    SigninDeepLinkFlowStrings.alreadySignedInMessage(
                            context, assumeNonNull(targetAccountInfo), payload);
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
            return;
        }

        FullscreenSigninAndHistorySyncConfig config =
                primaryAccountInfo == null
                        ? SigninDeepLinkFlowStrings.signinConfig(
                                /* context= */ context, /* targetEmail= */ payload.getEmail())
                        : SigninDeepLinkFlowStrings.switchAccountConfig(
                                /* context= */ context,
                                /* signedInEmail= */ primaryAccountInfo.getEmail(),
                                /* targetEmail= */ payload.getEmail());

        @Nullable Intent intent =
                SigninAndHistorySyncActivityLauncherImpl.get()
                        .createFullscreenSigninIntentOrShowError(
                                context, profile, config, SigninAccessPoint.DEEP_LINK_DEFAULT);
        if (intent == null) {
            SigninMetricsUtils.recordCrossDeviceInitialState(
                    payload.getExternalEntryPoint(), CrossDeviceInitialState.FLOW_FORBIDDEN);
            return;
        }

        recordCrossDeviceFlowStart(
                payload.getExternalEntryPoint(), primaryAccountInfo, targetAccountInfo);
        context.startActivity(intent);
    }

    private static void recordCrossDeviceFlowStart(
            @ExternalEntryPoint int entryPoint,
            @Nullable CoreAccountInfo primaryAccountInfo,
            @Nullable AccountInfo targetAccountInfo) {
        @CrossDeviceInitialState int initialState;
        if (primaryAccountInfo != null) {
            initialState =
                    targetAccountInfo != null
                            ? CrossDeviceInitialState
                                    .SIGNED_IN_WITH_DIFFERENT_ACCOUNT_TARGET_ACCOUNT_ON_DEVICE
                            : CrossDeviceInitialState
                                    .SIGNED_IN_WITH_DIFFERENT_ACCOUNT_TARGET_ACCOUNT_NOT_ON_DEVICE;
        } else {
            initialState =
                    targetAccountInfo != null
                            ? CrossDeviceInitialState.SIGNED_OUT_TARGET_ACCOUNT_ON_DEVICE
                            : CrossDeviceInitialState.SIGNED_OUT_TARGET_ACCOUNT_NOT_ON_DEVICE;
        }
        SigninMetricsUtils.recordCrossDeviceInitialState(entryPoint, initialState);
    }

    private SigninBridge() {}

    private static final class SigninDeepLinkFlowStrings {

        static String alreadySignedInMessage(
                Context context, AccountInfo account, SigninDeepLinkPayload payload) {
            var name =
                    !TextUtils.isEmpty(account.getGivenName())
                            ? account.getGivenName()
                            : payload.getEmail();
            return context.getString(
                    R.string.signin_deep_link_flow_already_signed_in_toast_message, name);
        }

        static FullscreenSigninAndHistorySyncConfig signinConfig(
                Context context, String targetEmail) {
            return FullscreenSigninAndHistorySyncConfig.builder(
                            context.getString(R.string.signin_deep_link_flow_signin_title),
                            context.getString(R.string.signin_deep_link_flow_signin_subtitle),
                            context.getString(R.string.signin_deep_link_flow_signin_dismiss_button),
                            context.getString(R.string.history_sync_title),
                            context.getString(R.string.history_sync_subtitle))
                    .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                    .selectedAccountEmail(targetEmail)
                    .build();
        }

        static FullscreenSigninAndHistorySyncConfig switchAccountConfig(
                Context context, String signedInEmail, String targetEmail) {
            return FullscreenSigninAndHistorySyncConfig.builderForSwitchAccountFlow(
                            context.getString(R.string.signin_deep_link_flow_switch_account_title),
                            context.getString(
                                    R.string.signin_deep_link_flow_switch_account_subtitle,
                                    signedInEmail,
                                    targetEmail),
                            context.getString(
                                    R.string.signin_deep_link_flow_switch_account_dismiss_button),
                            context.getString(R.string.history_sync_title),
                            context.getString(R.string.history_sync_subtitle),
                            targetEmail)
                    .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                    .build();
        }

        private SigninDeepLinkFlowStrings() {}
    }
}
