// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.WebSigninAccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** The bridge regroups methods invoked by native code to interact with Android Signin UI. */
final class SigninBridge {
    /** Used for dependency injection in unit tests. */
    @VisibleForTesting
    static class AccountPickerBottomSheetCoordinatorFactory {
        AccountPickerBottomSheetCoordinator create(
                WindowAndroid windowAndroid,
                BottomSheetController bottomSheetController,
                AccountPickerDelegate accountPickerDelegate,
                AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
                DeviceLockActivityLauncher deviceLockActivityLauncher,
                @AccountPickerLaunchMode int accountPickerLaunchMode) {
            return new AccountPickerBottomSheetCoordinator(
                    windowAndroid,
                    bottomSheetController,
                    accountPickerDelegate,
                    accountPickerBottomSheetStrings,
                    deviceLockActivityLauncher,
                    accountPickerLaunchMode,
                    /* isWebSignin= */ true,
                    SigninAccessPoint.WEB_SIGNIN);
        }
    }

    @VisibleForTesting static final int ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT = 3;

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
    private static void openAccountPickerBottomSheet(Tab tab, String continueUrl) {
        openAccountPickerBottomSheet(
                tab, continueUrl, new AccountPickerBottomSheetCoordinatorFactory());
    }

    /** Opens account picker bottom sheet. */
    @VisibleForTesting
    static void openAccountPickerBottomSheet(
            Tab tab, String continueUrl, AccountPickerBottomSheetCoordinatorFactory factory) {
        ThreadUtils.assertOnUiThread();
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null || !tab.isUserInteractable()) {
            // The page is opened in the background, ignore the header. See
            // https://crbug.com/1145031#c5 and https://crbug.com/323424409 for details.
            return;
        }
        Profile profile = tab.getProfile();
        SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(profile.getOriginalProfile());
        if (!signinManager.isSyncOptInAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                    SigninAccessPoint.WEB_SIGNIN);
            return;
        }
        final List<CoreAccountInfo> coreAccountInfos =
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
        if (coreAccountInfos.isEmpty()) {
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
        // TODO(b/41493784): Update this when the new sign-in flow will be used for the web signin
        // entry point.
        int titleId =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? R.string.signin_account_picker_bottom_sheet_title
                        : R.string.signin_account_picker_dialog_title;
        int subtitleId =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? R.string.signin_account_picker_bottom_sheet_subtitle_for_web_signin
                        : R.string.signin_account_picker_bottom_sheet_subtitle;
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(titleId)
                        .setSubtitleStringId(subtitleId)
                        .setDismissButtonStringId(R.string.signin_account_picker_dismiss_button)
                        .build();

        factory.create(
                windowAndroid,
                bottomSheetController,
                new WebSigninAccountPickerDelegate(tab, new WebSigninBridge.Factory(), continueUrl),
                strings,
                DeviceLockActivityLauncherImpl.get(),
                AccountPickerLaunchMode.DEFAULT);
    }

    private SigninBridge() {}
}
