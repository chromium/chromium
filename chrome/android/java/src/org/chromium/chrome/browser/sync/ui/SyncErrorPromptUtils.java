// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Shared code between the old {@link org.chromium.chrome.browser.infobar.SyncErrorInfobar}
 * and the new UI based on {@link SyncErrorMessage}.
 *
 * TODO(crbug.com/1355876): make private as methods of message ui controller once the migration to
 *                          the new UI is completed.
 */
public class SyncErrorPromptUtils {
    @VisibleForTesting
    public static final long MINIMAL_DURATION_BETWEEN_UI_MS =
            TimeUnit.MILLISECONDS.convert(24, TimeUnit.HOURS);

    @VisibleForTesting
    public static final long MINIMAL_DURATION_TO_PWM_ERROR_UI_MS =
            TimeUnit.MILLISECONDS.convert(30, TimeUnit.MINUTES);

    @IntDef({SyncErrorPromptType.NOT_SHOWN, SyncErrorPromptType.AUTH_ERROR,
            SyncErrorPromptType.PASSPHRASE_REQUIRED, SyncErrorPromptType.SYNC_SETUP_INCOMPLETE,
            SyncErrorPromptType.CLIENT_OUT_OF_DATE,
            SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
            SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS,
            SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING,
            SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncErrorPromptType {
        int NOT_SHOWN = -1;
        int AUTH_ERROR = 0;
        int PASSPHRASE_REQUIRED = 1;
        int SYNC_SETUP_INCOMPLETE = 2;
        int CLIENT_OUT_OF_DATE = 3;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING = 4;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS = 5;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING = 6;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS = 7;
    }

    @SyncErrorPromptType
    public static int getSyncErrorUiType(@SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
                return SyncErrorPromptType.AUTH_ERROR;
            case SyncError.PASSPHRASE_REQUIRED:
                return SyncErrorPromptType.PASSPHRASE_REQUIRED;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return SyncErrorPromptType.SYNC_SETUP_INCOMPLETE;
            case SyncError.CLIENT_OUT_OF_DATE:
                return SyncErrorPromptType.CLIENT_OUT_OF_DATE;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS;
            default:
                return SyncErrorPromptType.NOT_SHOWN;
        }
    }

    public static boolean shouldShowPrompt(@SyncErrorPromptType int type) {
        if (type == SyncErrorPromptType.NOT_SHOWN) {
            return false;
        }
        long lastShownTime =
                SharedPreferencesManager.getInstance().readLong(SYNC_ERROR_PROMPT_SHOWN_AT_TIME, 0);

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
                SYNC_ERROR_PROMPT_SHOWN_AT_TIME, TimeUtils.currentTimeMillis());
    }

    public static void resetLastShownTime() {
        SharedPreferencesManager.getInstance().removeKey(SYNC_ERROR_PROMPT_SHOWN_AT_TIME);
    }
}
