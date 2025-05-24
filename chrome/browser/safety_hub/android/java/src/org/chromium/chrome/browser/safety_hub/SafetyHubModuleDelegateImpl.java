// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** An implementation of {@link SafetyHubModuleDelegate} */
@NullMarked
public class SafetyHubModuleDelegateImpl implements SafetyHubModuleDelegate {
    private static final int INVALID_PASSWORD_COUNT = -1;
    private final Profile mProfile;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final SigninAndHistorySyncActivityLauncher mSigninLauncher;
    private final SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    /**
     * @param profile A supplier for {@link Profile} that owns the data being deleted.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} that will be used
     *     to launch the password check UI.
     * @param settingsCustomTabLauncher Used by the password manager dialogs to open a help center
     *     article in a CCT.
     */
    public SafetyHubModuleDelegateImpl(
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            SigninAndHistorySyncActivityLauncher signinLauncher,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSigninLauncher = signinLauncher;

        mSettingsCustomTabLauncher = settingsCustomTabLauncher;
    }

    @Override
    public void showPasswordCheckUi(Context context) {
        SafetyHubUtils.showPasswordCheckUi(
                context, mProfile, mModalDialogManagerSupplier, mSettingsCustomTabLauncher);
    }

    @Override
    public void showLocalPasswordCheckUi(Context context) {
        SafetyHubUtils.showLocalPasswordCheckUi(
                context, mProfile, mModalDialogManagerSupplier, mSettingsCustomTabLauncher);
    }

    @Override
    public void openGooglePlayStore(Context context) {
        if (!BuildConfig.IS_CHROME_BRANDED) {
            return;
        }

        String chromeAppId = context.getPackageName();
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(ContentUrlConstants.PLAY_STORE_URL_PREFIX + chromeAppId));

        context.startActivity(intent);
    }

    @Override
    public int getAccountPasswordsCount(@Nullable PasswordStoreBridge passwordStoreBridge) {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (passwordStoreBridge == null
                || !PasswordManagerHelper.hasChosenToSyncPasswords(syncService)) {
            return INVALID_PASSWORD_COUNT;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)) {
            if (PasswordManagerUtilBridge.isPasswordManagerAvailable(UserPrefs.get(mProfile))) {
                return passwordStoreBridge.getPasswordStoreCredentialsCountForAccountStore();
            }
            return INVALID_PASSWORD_COUNT;
        }

        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        if (!passwordManagerHelper.canUseUpm()) {
            return INVALID_PASSWORD_COUNT;
        }

        if (usesSplitStoresAndUPMForLocal(UserPrefs.get(mProfile))) {
            return passwordStoreBridge.getPasswordStoreCredentialsCountForAccountStore();
        }
        // If using split stores is disabled, all passwords reside in the profile store.
        return passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore();
    }

    @Override
    public int getLocalPasswordsCount(@Nullable PasswordStoreBridge passwordStoreBridge) {
        if (passwordStoreBridge == null) {
            return INVALID_PASSWORD_COUNT;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)) {
            if (PasswordManagerUtilBridge.isPasswordManagerAvailable(UserPrefs.get(mProfile))) {
                return passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore();
            }
            return INVALID_PASSWORD_COUNT;
        }

        // There are two cases where a user has local passwords in the profile store:
        //    1. If split stores are in use for local passwords, then profile store stores local
        // passwords.
        //    2. If they're not in use, but the user is not syncing, then profile store stores
        // local passwords.
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        boolean isSyncingPasswords = PasswordManagerHelper.hasChosenToSyncPasswords(syncService);
        if (usesSplitStoresAndUPMForLocal(UserPrefs.get(mProfile)) || !isSyncingPasswords) {
            return passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore();
        }

        // If split stores for local passwords are not in use and the user is syncing, then the
        // profile store doesn't store local passwords.
        return 0;
    }

    @Override
    public void launchSigninPromo(Context context) {
        assert !SafetyHubUtils.isSignedIn(mProfile);
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .setSubtitleStringId(R.string.safety_check_passwords_error_signed_out)
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE)
                        .build();
        // Open the sign-in page.

        @Nullable Intent intent =
                mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                        context, mProfile, config, SigninAccessPoint.SAFETY_CHECK);
        if (intent != null) {
            context.startActivity(intent);
        }
    }
}
