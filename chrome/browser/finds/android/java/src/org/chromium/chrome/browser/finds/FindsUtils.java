// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Static utilities for Finds Notifications. */
@NullMarked
public class FindsUtils {
    // LINT.IfChange(FindsOptInPromoInteractionPrefs)
    public static final String FINDS_OPT_IN_PROMO_USER_INTERACTED =
            "finds.opt_in_promo.user_interacted";
    // Deprecated. Do not remove.
    public static final String FINDS_OPT_IN_PROMO_INTERACTED_COUNT =
            "finds.opt_in_promo.interacted_count";
    // Deprecated. Do not remove.
    public static final String FINDS_OPT_IN_PROMO_LAST_INTERACTED_TIMESTAMP =
            "finds.opt_in_promo.last_interacted_timestamp";
    public static final String FINDS_OPT_IN_PROMO_SHOWN_COUNT = "finds.opt_in_promo.shown_count";
    public static final String FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP =
            "finds.opt_in_promo.last_shown_timestamp";

    // LINT.ThenChange(//chrome/browser/finds/core/finds_pref_names.cc:FindsOptInPromoInteractionPrefs)

    /**
     * Checks if the Finds notification channel has been created.
     *
     * @param callback Callback to return true if the channel exists, false otherwise.
     */
    public static void isFindsChannelCreated(Callback<Boolean> callback) {
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChannelId.CHROME_FINDS,
                        (channel) -> {
                            callback.onResult(channel != null);
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
     * Launch the settings page for the Finds channel. Will default to app-level notifications page
     * if the app-level notifications setting are not enabled.
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

    /**
     * Sets data indicating a user interaction with the Finds opt-in promo occurred by incrementing
     * the interaction count and saving the current timestamp.
     *
     * @param profile The current user {@link Profile}.
     */
    public static void setOptInPromoInteracted(Profile profile) {
        PrefService prefs = UserPrefs.get(profile);
        prefs.setBoolean(FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
    }

    /**
     * Sets data indicating a user has seen the Finds opt-in promo.
     *
     * @param profile The current user {@link Profile}.
     */
    public static void setOptInPromoSeen(Profile profile) {
        PrefService prefs = UserPrefs.get(profile);
        int count = prefs.getInteger(FINDS_OPT_IN_PROMO_SHOWN_COUNT);
        prefs.setInteger(FINDS_OPT_IN_PROMO_SHOWN_COUNT, count + 1);
        prefs.setLong(FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP, System.currentTimeMillis());
    }
}
