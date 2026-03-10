// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Static utilities for Chrome Finds Notifications. */
@NullMarked
public class ChromeFindsUtils {
    @IntDef({
        ChromeFindsOptInState.FIRST_TIME,
        ChromeFindsOptInState.ENABLED,
        ChromeFindsOptInState.MANUALLY_DISABLED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChromeFindsOptInState {
        /** The notification channel has not been created yet. */
        int FIRST_TIME = 0;

        /** The notification channel exists and is enabled (along with app-level notifications). */
        int ENABLED = 1;

        /** The notification channel exists but is disabled (or app-level notifications are). */
        int MANUALLY_DISABLED = 2;
    }

    /**
     * Determine the current opt-in state for Chrome Finds notifications.
     *
     * @param callback Callback to return the detected {@link ChromeFindsOptInState}.
     */
    public static void getOptInState(Callback<Integer> callback) {
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChannelId.CHROME_FINDS,
                        (channel) -> {
                            if (channel == null) {
                                callback.onResult(ChromeFindsOptInState.FIRST_TIME);
                            } else if (NotificationProxyUtils.areNotificationsEnabled()
                                    && channel.getImportance()
                                            != NotificationManager.IMPORTANCE_NONE) {
                                callback.onResult(ChromeFindsOptInState.ENABLED);
                            } else {
                                callback.onResult(ChromeFindsOptInState.MANUALLY_DISABLED);
                            }
                        });
    }

    /**
     * Check if both app-level and finds notifications are enabled.
     *
     * @param callback Callback to return the result.
     */
    public static void areFindsNotificationsEnabled(Callback<Boolean> callback) {
        if (!NotificationProxyUtils.areNotificationsEnabled()) {
            callback.onResult(false);
            return;
        }
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.CHROME_FINDS,
                        (channel) -> {
                            if (channel != null
                                    && channel.getImportance()
                                            != NotificationManager.IMPORTANCE_NONE) {
                                callback.onResult(true);
                            } else {
                                callback.onResult(false);
                            }
                        });
    }

    /**
     * @return Whether the opt-in promo should be always shown for debugging.
     */
    @VisibleForTesting
    static boolean shouldAlwaysShowOptInPromo() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CHROME_FINDS, "always_show_opt_in_promo", false);
    }

    /**
     * Returns whether the bottom sheet should be shown. This will be checked by the tab helper
     * triggering the opt-in bottom sheet to show.
     */
    public static boolean shouldShowOptInPromo() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_FINDS)) {
            return false;
        }
        if (shouldAlwaysShowOptInPromo()) {
            return true;
        }
        return !ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.CHROME_FINDS_OPT_IN_PROMO_DECLINED, false);
    }

    /**
     * Launch the settings page for the Chrome Finds channel. Will default to app-level
     * notifications page if the app-level notifications setting are not enabled.
     *
     * @param context The current context.
     */
    public static void launchFindsNotificationSettings(Context context) {
        Intent intent = new Intent();
        if (NotificationProxyUtils.areNotificationsEnabled()) {
            intent.setAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
            intent.putExtra(Settings.EXTRA_CHANNEL_ID, ChannelId.CHROME_FINDS);
        } else {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}
