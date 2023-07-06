// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.accounts.Account;
import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Helps sign in and enable sync in the background if appropriate after a backup restore.
 * Usage:
 * BackupSigninProcessor.start(activity).
 * TODO(crbug.com/1318463): Consider signing in immediately instead of lazily.
 */
public final class BackupSigninProcessor {
    /**
     * Initiates the automatic sign-in process in background. Must only be called once native is
     * initialized.
     *
     * @param activity The current activity.
     */
    public static void start(final Activity activity) {
        // Warning, another layer relies on SigninManager being instantiated at this moment, don't
        // move this call around.
        // TODO(crbug.com/1336196): Delete comment above once the dependency is gone.
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        final String accountName = getBackupFlowSigninAccountName();
        if (!signinManager.isSyncOptInAllowed() || TextUtils.isEmpty(accountName)) {
            setBackupFlowSigninComplete();
            return;
        }

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.getAccounts().then(accounts -> {
            AccountUtils.checkChildAccountStatusLegacy(
                    accountManagerFacade, accounts, (isChild, unused) -> {
                        if (isChild) {
                            // TODO(crbug.com/1318350): Pre-AllowSyncOffForChildAccounts, the backup
                            // sign-in for child accounts would happen in SigninChecker anyways.
                            // Maybe it should be handled by this class once the feature launches.
                            setBackupFlowSigninComplete();
                            return;
                        }

                        Account account = AccountUtils.findAccountByName(accounts, accountName);
                        if (account == null) {
                            setBackupFlowSigninComplete();
                            return;
                        }

                        signinAndEnableSync(account, activity);
                    });
        });
    }

    private static void signinAndEnableSync(@NonNull Account account, Activity activity) {
        Profile profile = Profile.getLastUsedRegularProfile();
        IdentityServicesProvider.get().getSigninManager(profile).signinAndEnableSync(account,
                SigninAccessPoint.POST_DEVICE_RESTORE_BACKGROUND_SIGNIN, new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                profile, true);
                        SyncServiceFactory.getForProfile(profile)
                                .setInitialSyncFeatureSetupComplete(
                                        SyncFirstSetupCompleteSource.ANDROID_BACKUP_RESTORE);
                        setBackupFlowSigninComplete();
                    }

                    @Override
                    public void onSignInAborted() {
                        // If sign-in failed, give up and mark as complete.
                        // TODO(crbug.com/1371130): Measure how often this happens.
                        setBackupFlowSigninComplete();
                    }
                });
    }

    /**
     * Marks the backup flow sign-in as complete (whether sign-in was indeed performed or not).
     */
    private static void setBackupFlowSigninComplete() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME);
    }

    /**
     * @return The account name restored during the backup flow, or null if none.
     */
    private static String getBackupFlowSigninAccountName() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME, null);
    }
}
