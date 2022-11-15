// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/**
 * Tracks the last time the sync error message was shown and decides whether that was long enough
 * ago to show the message again.
 */
public class SyncErrorMessageImpressionTracker {
    @VisibleForTesting
    public static final long MINIMAL_DURATION_BETWEEN_UI_MS =
            TimeUnit.MILLISECONDS.convert(24, TimeUnit.HOURS);

    @VisibleForTesting
    public static final long MINIMAL_DURATION_TO_PWM_ERROR_UI_MS =
            TimeUnit.MILLISECONDS.convert(30, TimeUnit.MINUTES);

    public static boolean canShowNow() {
        long lastShownTime = SharedPreferencesManager.getInstance().readLong(
                SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, 0);

        if (ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)) {
            // Since the password manager error and the sync error can be related,
            // the sync error should be shown only if at least MINIMAL_DURATION_TO_PWM_ERROR_UI_MS
            // have passed since the last password manager error. This condition is mirrored
            // for the password manager error.
            long currentTime = TimeUtils.currentTimeMillis();
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            long upmErrorShownTime =
                    Long.valueOf(prefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP));
            return currentTime - lastShownTime > MINIMAL_DURATION_BETWEEN_UI_MS
                    && currentTime - upmErrorShownTime > MINIMAL_DURATION_TO_PWM_ERROR_UI_MS;
        }
        return TimeUtils.currentTimeMillis() - lastShownTime > MINIMAL_DURATION_BETWEEN_UI_MS;
    }

    public static void updateLastShownTime() {
        SharedPreferencesManager.getInstance().writeLong(
                SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, TimeUtils.currentTimeMillis());
    }

    public static void resetLastShownTime() {
        SharedPreferencesManager.getInstance().removeKey(SYNC_ERROR_MESSAGE_SHOWN_AT_TIME);
    }
}
