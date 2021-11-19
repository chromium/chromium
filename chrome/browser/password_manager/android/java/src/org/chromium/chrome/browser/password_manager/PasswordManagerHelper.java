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
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
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
            IdentityManager identityManager, SyncService syncService) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);
        if (credentialManagerLauncher != null) {
            if (isSyncingPasswords(identityManager, syncService)) {
                credentialManagerLauncher.getCredentialManagerLaunchIntentForAccount(referrer,
                        CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()),
                        PasswordManagerHelper::launchCredentialManager,
                        PasswordManagerHelper::recordFailureMetrics);
            } else {
                credentialManagerLauncher.getCredentialManagerLaunchIntentForLocal(referrer,
                        PasswordManagerHelper::launchCredentialManager,
                        PasswordManagerHelper::recordFailureMetrics);
            }
            return;
        }

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
        context.startActivity(settingsLauncher.createSettingsActivityIntent(
                context, PASSWORD_SETTINGS_CLASS, fragmentArgs));
    }

    public static boolean isSyncingPasswords(
            IdentityManager identityManager, SyncService syncService) {
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SYNC)) return false;
        if (syncService == null
                || !syncService.getActiveDataTypes().contains(ModelType.PASSWORDS)) {
            return false;
        }
        return true;
    }

    public static boolean isSyncingPasswordsWithoutCustomPassphrase(
            IdentityManager identityManager, SyncService syncService) {
        if (!PasswordManagerHelper.isSyncingPasswords(identityManager, syncService)) return false;
        if (syncService == null || syncService.isUsingExplicitPassphrase()) return false;
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
