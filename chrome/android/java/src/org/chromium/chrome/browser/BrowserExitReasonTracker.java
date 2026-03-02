// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.SharedPreferences;
import android.os.Build;
import android.os.Process;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;

/** Records the SystemExitReason for the Browser process. */
@NullMarked
public class BrowserExitReasonTracker {
    private BrowserExitReasonTracker() {}

    public static void initForegroundBrowserProcess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        if (prefs.contains(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON)) {
            int reason = prefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON);
            ProcessExitReasonFromSystem.recordAsEnumHistogram(
                    "Stability.Android.SystemExitReason.Browser2", reason);
        }
        SharedPreferences.Editor ed = prefs.getEditor();
        ed.remove(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON);
        // Store current PID for next session to detect how this one exited.
        ed.putInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID, Process.myPid());
        ed.apply();
    }

    public static void onBrowserProcessCreated() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int previousPid = prefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID);
        if (previousPid != 0) {
            int reason = ProcessExitReasonFromSystem.getExitReason(previousPid);
            SharedPreferences.Editor ed = prefs.getEditor();
            ed.remove(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID);
            // If this is a background launch eg. JobScheduler, we may not load native and persist
            // histograms. Store the histrogram value to shared prefs to record it on next
            // foreground launch.
            ed.putInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON, reason);
            ed.apply();
        }
    }
}
