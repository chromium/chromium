// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Bridge between Java and native PasswordManager code.
 */
public class PasswordManagerLauncher {
    private PasswordManagerLauncher() {}

    /**
     * Launches the password settings.
     *
     * @param activity used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(
            Activity activity, @ManagePasswordsReferrer int referrer) {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        SyncService syncService = SyncService.get();
        if (PasswordManagerHelper.isSyncingPasswordsWithoutCustomPassphrase(
                    identityManager, syncService)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_SCRIPTS_FETCHING)) {
            PasswordScriptsFetcherBridge.prewarmCache();
        }
        CredentialManagerLauncher credentialManagerLauncher = null;
        PasswordManagerHelper.showPasswordSettings(activity, referrer, new SettingsLauncherImpl(),
                CredentialManagerLauncherFactory.getInstance().createLauncher(), identityManager,
                syncService);
    }

    @CalledByNative
    private static void showPasswordSettings(
            WebContents webContents, @ManagePasswordsReferrer int referrer) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        WeakReference<Activity> currentActivity = window.getActivity();
        showPasswordSettings(currentActivity.get(), referrer);
    }
}
