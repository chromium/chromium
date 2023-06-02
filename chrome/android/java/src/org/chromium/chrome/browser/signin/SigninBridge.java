// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninAccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * The bridge regroups methods invoked by native code to interact with Android Signin UI.
 */
final class SigninBridge {
    @VisibleForTesting
    static final int ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT = 3;

    /**
     * Launches {@link SyncConsentActivity}.
     * @param windowAndroid WindowAndroid from which to get the Context.
     * @param accessPoint for metrics purposes.
     */
    @CalledByNative
    private static void launchSigninActivity(
            WindowAndroid windowAndroid, @SigninAccessPoint int accessPoint) {
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            SyncConsentActivityLauncherImpl.get().launchActivityIfAllowed(context, accessPoint);
        }
    }

    /**
     * Opens account management screen.
     */
    @CalledByNative
    private static void openAccountManagementScreen(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            AccountManagementFragment.openAccountManagementScreen(context, gaiaServiceType);
        }
    }

    /**
     * Opens account picker bottom sheet.
     */
    @VisibleForTesting
    @CalledByNative
    static void openAccountPickerBottomSheet(WindowAndroid windowAndroid, String continueUrl) {
        ThreadUtils.assertOnUiThread();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (!signinManager.isSyncOptInAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED);
            return;
        }
        final List<Account> accounts = AccountUtils.getAccountsIfFulfilledOrEmpty(
                AccountManagerFacadeProvider.getInstance().getAccounts());
        if (accounts.isEmpty()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS);
            return;
        }
        if (SigninPreferencesManager.getInstance().getWebSigninAccountPickerActiveDismissalCount()
                >= ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS);
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
        // To close the current regular tab after the user clicks on "Continue" in the incognito
        // interstitial.
        final Supplier<TabModelSelector> tabModelSelectorSupplier =
                TabModelSelectorSupplier.from(windowAndroid);
        assert tabModelSelectorSupplier.hasValue() : "No TabModelSelector available.";
        final TabModel regularTabModel =
                tabModelSelectorSupplier.get().getModel(/*incognito=*/false);
        new AccountPickerBottomSheetCoordinator(windowAndroid, bottomSheetController,
                new WebSigninAccountPickerDelegate(TabModelUtils.getCurrentTab(regularTabModel),
                        new WebSigninBridge.Factory(), continueUrl),
                new AccountPickerBottomSheetStrings() {}, DeviceLockActivityLauncherImpl.get());
    }

    private SigninBridge() {}
}
