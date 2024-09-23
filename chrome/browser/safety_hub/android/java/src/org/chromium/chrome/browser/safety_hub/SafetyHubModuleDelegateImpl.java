// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** An implementation of {@link SafetyHubModuleDelegate} */
public class SafetyHubModuleDelegateImpl implements SafetyHubModuleDelegate {
    private static final int INVALID_PASSWORD_COUNT = -1;
    private final @NonNull Profile mProfile;
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final @NonNull SigninAndHistorySyncActivityLauncher mSigninLauncher;
    private final @NonNull SyncConsentActivityLauncher mSyncLauncher;

    /**
     * @param profile A supplier for {@link Profile} that owns the data being deleted.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} that will be used
     *     to launch the password check UI.
     */
    public SafetyHubModuleDelegateImpl(
            @NonNull Profile profile,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull SigninAndHistorySyncActivityLauncher signinLauncher,
            @NonNull SyncConsentActivityLauncher syncLauncher) {
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSigninLauncher = signinLauncher;
        mSyncLauncher = syncLauncher;
    }

    @Override
    public void showPasswordCheckUI(Context context) {
        SafetyHubUtils.showPasswordCheckUI(context, mProfile, mModalDialogManagerSupplier);
    }

    @Override
    public @Nullable UpdateStatusProvider.UpdateStatus getUpdateStatus() {
        return SafetyHubFetchServiceFactory.getForProfile(mProfile).getUpdateStatus();
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
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (passwordStoreBridge == null
                || !PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                || !passwordManagerHelper.canUseUpm()) return INVALID_PASSWORD_COUNT;

        if (usesSplitStoresAndUPMForLocal(UserPrefs.get(mProfile))) {
            return passwordStoreBridge.getPasswordStoreCredentialsCountForAccountStore();
        }
        // If using split stores is disabled, all passwords reside in the profile store.
        return passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore();
    }

    @Override
    public void launchSyncOrSigninPromo(Context context) {
        assert !SafetyHubUtils.isSignedIn(mProfile);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            AccountPickerBottomSheetStrings strings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    R.string.signin_account_picker_bottom_sheet_title)
                            .setSubtitleStringId(R.string.safety_check_passwords_error_signed_out)
                            .build();
            // Open the sign-in page.
            mSigninLauncher.launchActivityIfAllowed(
                    context,
                    mProfile,
                    strings,
                    SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                    SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
                    SigninAccessPoint.SAFETY_CHECK);
        } else {
            // Open the sync page.
            mSyncLauncher.launchActivityIfAllowed(context, SigninAccessPoint.SAFETY_CHECK);
        }
    }
}
