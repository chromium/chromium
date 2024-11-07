// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Record the results of showing a clear data dialog on TWA client uninstall or data clear. */
public class ClearDataDialogResultRecorder {
    private ClearDataDialogResultRecorder() {}

    /**
     * Called when the dialog is closed.
     *
     * @param accepted Whether positive button was clicked.
     * @param triggeredByUninstall Whether the dialog was triggered by uninstall.
     */
    public static void handleDialogResult(boolean accepted, boolean triggeredByUninstall) {
        if (accepted || ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
            // If accepted, native is going to be loaded for the settings.
            TrustedWebActivityUmaRecorder.recordClearDataDialogAction(
                    accepted, triggeredByUninstall);
        } else {
            // Avoid loading native just for the sake of recording. Save the info and record
            // on next Chrome launch.
            String key =
                    triggeredByUninstall
                            ? TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL
                            : TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA;
            SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
            prefs.writeInt(key, prefs.readInt(key) + 1);
        }
    }

    /** Make recordings that were deferred in order to not load native. */
    public static void makeDeferredRecordings() {
        recordDismissals(TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL, true);
        recordDismissals(TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA, false);
    }

    private static void recordDismissals(String prefKey, boolean triggeredByUninstall) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int times = prefs.readInt(prefKey);
        for (int i = 0; i < times; i++) {
            TrustedWebActivityUmaRecorder.recordClearDataDialogAction(
                    /* accepted= */ false, triggeredByUninstall);
        }
        prefs.removeKey(prefKey);
    }
}
