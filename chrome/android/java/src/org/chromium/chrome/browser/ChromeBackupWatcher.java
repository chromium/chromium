// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.backup.BackupManager;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.signin.ChromeSigninController;

/**
 * Class for watching for changes to the Android preferences that are backed up using Android
 * key/value backup.
 */
@JNINamespace("android")
public class ChromeBackupWatcher {
    private BackupManager mBackupManager;

    private static final String FIRST_BACKUP_DONE = "first_backup_done";

    @VisibleForTesting
    @CalledByNative
    static ChromeBackupWatcher createChromeBackupWatcher() {
        return new ChromeBackupWatcher();
    }

    private ChromeBackupWatcher() {
        Context context = ContextUtils.getApplicationContext();
        if (context == null) return;

        mBackupManager = new BackupManager(context);
        // Watch the Java preferences that are backed up.
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        // If we have never done a backup do one immediately.
        if (!sharedPrefs.getBoolean(FIRST_BACKUP_DONE, false)) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mBackupManager.dataChanged();
            }
            sharedPrefs.edit().putBoolean(FIRST_BACKUP_DONE, true).apply();
        }
        sharedPrefs.registerOnSharedPreferenceChangeListener(
                (sharedPreferences, key) -> {
                    // Update the backup if the user id or any of the backed up Android
                    // preferences change.
                    if (key.equals(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY)) {
                        onBackupPrefsChanged();
                        return;
                    }
                    for (String pref : ChromeBackupAgent.BACKUP_ANDROID_BOOL_PREFS) {
                        if (key.equals(pref)) {
                            onBackupPrefsChanged();
                            return;
                        }
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
