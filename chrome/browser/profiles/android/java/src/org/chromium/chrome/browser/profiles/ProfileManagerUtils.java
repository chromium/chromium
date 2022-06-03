// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.os.SystemClock;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A utility class for applying operations on all loaded profiles.
 */
public class ProfileManagerUtils {
    private static final long BOOT_TIMESTAMP_MARGIN_MS = 1000;

    /**
     * Commits pending writes for all loaded profiles. The host activity should call this during its
     * onPause() handler to ensure all state is saved when the app is suspended.
     */
    public static void flushPersistentDataForAllProfiles() {
        try {
            TraceEvent.begin("ProfileManagerUtils.commitPendingWritesForAllProfiles");
            ProfileManagerUtilsJni.get().flushPersistentDataForAllProfiles();
        } finally {
            TraceEvent.end("ProfileManagerUtils.commitPendingWritesForAllProfiles");
        }
    }

    /**
     * Removes all session cookies (cookies with no expiration date). This should be called after
     * device reboots. This function incorrectly clears cookies when Daylight Savings Time changes
     * the clock. Without a way to get a monotonically increasing system clock, the boot timestamp
     * will be off by one hour. However, this should only happen at most once when the clock changes
     * since the updated timestamp is immediately saved.
     */
    public static void removeSessionCookiesForAllProfiles() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        long lastKnownBootTimestamp =
                preferences.readLong(ChromePreferenceKeys.PROFILES_BOOT_TIMESTAMP, 0);
        long bootTimestamp = System.currentTimeMillis() - SystemClock.uptimeMillis();
        long difference = bootTimestamp - lastKnownBootTimestamp;

        // Allow some leeway to account for fractions of milliseconds.
        if (Math.abs(difference) > BOOT_TIMESTAMP_MARGIN_MS) {
            ProfileManagerUtilsJni.get().removeSessionCookiesForAllProfiles();

            preferences.writeLong(ChromePreferenceKeys.PROFILES_BOOT_TIMESTAMP, bootTimestamp);
        }
    }

    @NativeMethods
    interface Natives {
        void flushPersistentDataForAllProfiles();
        void removeSessionCookiesForAllProfiles();
    }
}
