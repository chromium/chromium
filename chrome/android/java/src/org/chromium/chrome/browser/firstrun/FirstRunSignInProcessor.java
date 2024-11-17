// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** A helper to delay opening sync settings for an FRE advanced sync setup. */
public final class FirstRunSignInProcessor {
    /**
     * If scheduleOpeningSettings() was previously called, this will open sync settings so the user
     * can complete their advanced sync setup.
     * @param activity The context for the FRE parameters processor.
     */
    public static void openSyncSettingsIfScheduled(Activity activity) {
        final SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        if (prefs.readBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, false)) {
            prefs.removeKey(ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP);
            SettingsNavigation settingsNavigation =
                    SettingsNavigationFactory.createSettingsNavigation();
            settingsNavigation.startSettings(
                    activity, ManageSyncSettings.class, ManageSyncSettings.createArguments(true));
        }
    }

    /** Sets the preference to schedule opening sync settings after the FRE finishes. */
    public static void scheduleOpeningSettings() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, true);
    }
}
