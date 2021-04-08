// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerDelegateImpl;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerFeatureUtils;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManagerSupplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/**
 * The bridge regroups methods invoked by native code to interact with Android Signin UI.
 */
final class SigninBridge {
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
            SigninActivityLauncherImpl.get().launchActivityIfAllowed(context, accessPoint);
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
        if (!signinManager.isSignInAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED);
            return;
        }
        if (AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts().isEmpty()) {
            // TODO(https://crbug.com/1119720): Show the bottom sheet when no accounts on device
            //  in the future. This disabling is only temporary.
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS);
            return;
        }
        if (SigninPreferencesManager.getInstance().getAccountPickerBottomSheetActiveDismissalCount()
                >= AccountPickerFeatureUtils.getDismissLimit()) {
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
        // To create a new incognito tab after after the user clicks on "Continue" in the incognito
        // interstitial.
        final Supplier<TabCreatorManager> tabCreatorManagerSupplier =
                TabCreatorManagerSupplier.from(windowAndroid);
        assert tabCreatorManagerSupplier.hasValue() : "No TabCreatorManager available.";
        final TabCreator incognitoTabCreator =
                tabCreatorManagerSupplier.get().getTabCreator(/*incognito=*/true);
        new AccountPickerBottomSheetCoordinator(windowAndroid.getActivity().get(),
                bottomSheetController,
                new AccountPickerDelegateImpl(windowAndroid,
                        TabModelUtils.getCurrentTab(regularTabModel), new WebSigninBridge.Factory(),
                        continueUrl),
                regularTabModel, incognitoTabCreator, HelpAndFeedbackLauncherImpl.getInstance(),
                /* showIncognitoRow= */ IncognitoUtils.isIncognitoModeEnabled());
    }

    private SigninBridge() {}
}
