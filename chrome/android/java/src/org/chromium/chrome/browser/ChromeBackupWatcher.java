// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.backup.BackupManager;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

/**
 * Class for watching for changes to the Android preferences that are backed up using Android
 * key/value backup.
 */
@JNINamespace("android")
public class ChromeBackupWatcher {
    private BackupManager mBackupManager;

    @VisibleForTesting
    @CalledByNative
    static ChromeBackupWatcher createChromeBackupWatcher() {
        return new ChromeBackupWatcher();
    }

    // Suppress to observe SharedPreferences, which is discouraged; use another messaging channel
    // instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    private ChromeBackupWatcher() {
        Context context = ContextUtils.getApplicationContext();
        if (context == null) return;

        mBackupManager = new BackupManager(context);
        // Watch the Java preferences that are backed up.
        SharedPreferencesManager sharedPrefs = ChromeSharedPreferences.getInstance();
        // If we have never done a backup do one immediately.
        if (!sharedPrefs.readBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, false)) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mBackupManager.dataChanged();
            }
            sharedPrefs.writeBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, true);
        }
        ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(
                (prefs, key) -> {
                    // Update the backup if any of the backed up Android preferences change.
                    for (String pref : ChromeBackupAgentImpl.BACKUP_ANDROID_BOOL_PREFS) {
                        if (key.equals(pref)) {
                            onBackupPrefsChanged();
                            return;
                        }
                    }
                });
        // Update the backup if the sign-in status changes.
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        identityManager.addObserver(new IdentityManager.Observer() {
            @Override
            public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
                onBackupPrefsChanged();
            }
        });
    }

    @CalledByNative
    private void onBackupPrefsChanged() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mBackupManager.dataChanged();
        }
    }
}
