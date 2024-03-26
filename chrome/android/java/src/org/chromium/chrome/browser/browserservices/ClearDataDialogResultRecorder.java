// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL;

import dagger.Lazy;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

import javax.inject.Inject;

/** Record the results of showing a clear data dialog on TWA client uninstall or data clear. */
public class ClearDataDialogResultRecorder {
    private final Lazy<SharedPreferencesManager> mPrefsManager;
    private final ChromeBrowserInitializer mBrowserInitializer;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    @Inject
    public ClearDataDialogResultRecorder(
            Lazy<SharedPreferencesManager> manager,
            ChromeBrowserInitializer browserInitializer,
            TrustedWebActivityUmaRecorder umaRecorder) {
        mPrefsManager = manager;
        mBrowserInitializer = browserInitializer;
        mUmaRecorder = umaRecorder;
    }

    /**
     * Called when the dialog is closed.
     * @param accepted Whether positive button was clicked.
     * @param triggeredByUninstall Whether the dialog was triggered by uninstall.
     */
    public void handleDialogResult(boolean accepted, boolean triggeredByUninstall) {
        if (accepted || mBrowserInitializer.isFullBrowserInitialized()) {
            // If accepted, native is going to be loaded for the settings.
            mBrowserInitializer.runNowOrAfterFullBrowserStarted(
                    () -> mUmaRecorder.recordClearDataDialogAction(accepted, triggeredByUninstall));
        } else {
            // Avoid loading native just for the sake of recording. Save the info and record
            // on next Chrome launch.
            String key =
                    triggeredByUninstall
                            ? TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL
                            : TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA;

            mPrefsManager.get().writeInt(key, mPrefsManager.get().readInt(key) + 1);
        }
    }

    /** Make recordings that were deferred in order to not load native. */
    public void makeDeferredRecordings() {
        recordDismissals(TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL, true);
        recordDismissals(TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA, false);
    }

    private void recordDismissals(String prefKey, boolean triggeredByUninstall) {
        int times = mPrefsManager.get().readInt(prefKey);
        for (int i = 0; i < times; i++) {
            mUmaRecorder.recordClearDataDialogAction(/* accepted= */ false, triggeredByUninstall);
        }
        mPrefsManager.get().removeKey(prefKey);
    }
}
