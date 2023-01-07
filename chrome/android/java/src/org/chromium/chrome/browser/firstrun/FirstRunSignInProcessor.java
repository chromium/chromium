// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * A helper to delay opening sync settings for an FRE advanced sync setup.
 */
public final class FirstRunSignInProcessor {
    /**
     * If scheduleOpeningSettings() was previously called, this will open sync settings so the user
     * can complete their advanced sync setup.
     * @param activity The context for the FRE parameters processor.
     */
    public static void openSyncSettingsIfScheduled(Activity activity) {
        final SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        if (prefs.readBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, false)) {
            prefs.removeKey(ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP);
            // The FRE and backup background sign-ins used to mark completion via the same pref.
            // Keep writing this pref for a while to support rollbacks, i.e. so background sign-ins
            // don't suddently trigger because the pref isn't set.
            // TODO(crbug.com/1318463): Remove after crrev.com/c/3870839 reaches stable safely.
            prefs.writeBoolean(
                    ChromePreferenceKeys.LEGACY_FIRST_RUN_AND_BACKUP_SIGNIN_COMPLETE, true);
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(
                    activity, ManageSyncSettings.class, ManageSyncSettings.createArguments(true));
        }
    }

    /**
     * Sets the preference to schedule opening sync settings after the FRE finishes.
     */
    public static void scheduleOpeningSettings() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, true);
    }
}
