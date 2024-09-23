// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import android.app.backup.BackupManager;
import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

/**
 * Class for watching for changes to the Android preferences that are backed up using Android
 * key/value backup.
 */
@JNINamespace("android")
class ChromeBackupWatcher {
    private final BackupManager mBackupManager;
    private final PrefChangeRegistrar mPrefChangeRegistrar;

    // Suppress to observe SharedPreferences, which is discouraged; use another messaging channel
    // instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    @CalledByNative
    private ChromeBackupWatcher() {
        Context context = ContextUtils.getApplicationContext();
        assert context != null;

        mBackupManager = new BackupManager(context);
        // Watch the Java preferences that are backed up.
        SharedPreferencesManager sharedPrefs = ChromeSharedPreferences.getInstance();
        // If we have never done a backup do one immediately.
        if (!sharedPrefs.readBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, false)) {
            mBackupManager.dataChanged();
            sharedPrefs.writeBoolean(ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE, true);
        }
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(
                        (prefs, key) -> {
                            // Update the backup if any of the backed up Android preferences change.
                            for (String pref : ChromeBackupAgentImpl.BACKUP_ANDROID_BOOL_PREFS) {
                                if (key.equals(pref)) {
                                    onBackupPrefsChanged();
                                    return;
                                }
                            }
                        });

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        for (PrefBackupSerializer serializer : ChromeBackupAgentImpl.NATIVE_PREFS_SERIALIZERS) {
            for (String pref : serializer.getAllowlistedPrefs()) {
                mPrefChangeRegistrar.addObserver(pref, this::onBackupPrefsChanged);
            }
        }

        // Update the backup if the sign-in status changes.
        IdentityManager identityManager =
                IdentityServicesProvider.get()
                        .getIdentityManager(ProfileManager.getLastUsedRegularProfile());
        identityManager.addObserver(
                new IdentityManager.Observer() {
                    @Override
                    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
                        onBackupPrefsChanged();
                    }
                });
    }

    @CalledByNative
    private void destroy() {
        assert mPrefChangeRegistrar != null;
        mPrefChangeRegistrar.destroy();
    }

    private void onBackupPrefsChanged() {
        assert mBackupManager != null;
        mBackupManager.dataChanged();
    }
}
