// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;

/** A helper class for showing PasswordSettings. */
public class PasswordManagerHelper {
    // Key for the argument with which PasswordsSettings will be launched. The value for
    // this argument should be part of the ManagePasswordsReferrer enum, which contains
    // all points of entry to the passwords settings.
    public static final String MANAGE_PASSWORDS_REFERRER = "manage-passwords-referrer";

    // |PasswordSettings| full class name to open the fragment. Will be changed to
    // |PasswordSettings.class.getName()| once it's modularized.
    private static final String PASSWORD_SETTINGS_CLASS =
            "org.chromium.chrome.browser.password_manager.settings.PasswordSettings";

    /**
     * Launches the password settings or, if available, the credential manager from Google Play
     * Services.
     *
     * @param context used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(Context context, @ManagePasswordsReferrer int referrer,
            SettingsLauncher settingsLauncher, CredentialManagerLauncher credentialManagerLauncher,
            SyncService syncService) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);
        if (credentialManagerLauncher == null) {
            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
            context.startActivity(settingsLauncher.createSettingsActivityIntent(
                    context, PASSWORD_SETTINGS_CLASS, fragmentArgs));
            return;
        }

        if (hasChosenToSyncPasswords(syncService)) {
            credentialManagerLauncher.getCredentialManagerLaunchIntentForAccount(referrer,
                    CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()),
                    PasswordManagerHelper::launchCredentialManager,
                    PasswordManagerHelper::recordFailureMetrics);
        } else {
            credentialManagerLauncher.getCredentialManagerLaunchIntentForLocal(referrer,
                    PasswordManagerHelper::launchCredentialManager,
                    PasswordManagerHelper::recordFailureMetrics);
        }
    }

    /**
     *  Checks whether the sync feature is enabled and the user has chosen to sync passwords.
     *  Note that this doesn't mean that passwords are actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled
     */
    public static boolean hasChosenToSyncPasswords(SyncService syncService) {
        return syncService != null && syncService.isSyncFeatureEnabled()
                && syncService.getChosenDataTypes().contains(ModelType.PASSWORDS);
    }

    /**
     *  Checks whether the sync feature is enabled, the user has chosen to sync passwords and
     *  they haven't set up a custom passphrase.
     *  Note that this doesn't mean that passwords are actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled
     */
    public static boolean hasChosenToSyncPasswordsWithNoCustomPassphrase(SyncService syncService) {
        return PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                && !syncService.isUsingExplicitPassphrase();
    }

    /**
     * Checks whether the user is actively syncing passwords without a custom passphrase.
     *
     * @param syncService the service to query about the sync status.
     * @return true if actively syncing passwords and no custom passphrase was set.
     */
    public static boolean isSyncingPasswordsWithNoCustomPassphrase(SyncService syncService) {
        if (syncService == null || !syncService.hasSyncConsent()) return false;
        if (!syncService.getActiveDataTypes().contains(ModelType.PASSWORDS)) return false;
        if (syncService.isUsingExplicitPassphrase()) return false;
        return true;
    }

    private static void recordFailureMetrics(Exception exception) {
        // TODO(crbug.com/1255038): Record metrics.
    }

    private static void launchCredentialManager(PendingIntent intent) {
        try {
            intent.send();
        } catch (CanceledException e) {
            // TODO(crbug.com/1255038): Record metrics.
        }
    }
}
