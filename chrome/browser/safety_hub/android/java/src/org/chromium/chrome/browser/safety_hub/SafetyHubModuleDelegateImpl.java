// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** An implementation of {@link SafetyHubModuleDelegate} */
@NullMarked
public class SafetyHubModuleDelegateImpl implements SafetyHubModuleDelegate {
    private static final int INVALID_PASSWORD_COUNT = -1;
    private final Profile mProfile;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
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
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
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

        if (PasswordManagerUtilBridge.isPasswordManagerAvailable()) {
            return passwordStoreBridge.getPasswordStoreCredentialsCountForAccountStore();
        }
        return INVALID_PASSWORD_COUNT;
    }

    @Override
    public int getLocalPasswordsCount(@Nullable PasswordStoreBridge passwordStoreBridge) {
        if (passwordStoreBridge == null) {
            return INVALID_PASSWORD_COUNT;
        }

        if (PasswordManagerUtilBridge.isPasswordManagerAvailable()) {
            return passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore();
        }
        return INVALID_PASSWORD_COUNT;
    }

    @Override
    public void launchSigninPromo(Context context) {
        assert !SafetyHubUtils.isSignedIn(mProfile);
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(
                                context.getString(
                                        R.string.signin_account_picker_bottom_sheet_title))
                        .setSubtitleString(
                                context.getString(R.string.safety_check_passwords_error_signed_out))
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
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
