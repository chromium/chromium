// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/** Static utilities for Finds Notifications. */
@NullMarked
public class FindsUtils {
    // LINT.IfChange(FindsOptInPromoInteractionPrefs)
    public static final String FINDS_OPT_IN_PROMO_USER_INTERACTED =
            "finds.opt_in_promo.user_interacted";
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
     * Checks if the opt-in promo should be shown.
     *
     * @param profile The current user profile.
     * @param callback Callback to return true if the promo should be shown, false otherwise.
     */
    public static void checkShowCriteriaOptInPromo(Profile profile, Callback<Boolean> callback) {
        areFindsNotificationsEnabled(
                (notificationsEnabled) -> {
                    if (notificationsEnabled
                            || !FindsServiceJni.get().isHistorySyncAndMsbbEnabled(profile)
                            || !checkOptInPromoCriteria(profile)) {
                        callback.onResult(false);
                        return;
                    }

                    callback.onResult(true);
                });
    }

    private static boolean checkOptInPromoCriteria(Profile profile) {
        PrefService prefs = UserPrefs.get(profile);

        // Ensure that the promo has not been interacted with, otherwise do not show.
        boolean interacted = prefs.getBoolean(FINDS_OPT_IN_PROMO_USER_INTERACTED);
        if (interacted) {
            return false;
        }

        if (FindsFeatures.sAlwaysShowOptInPromo.getValue()) {
            return true;
        }

        // Check that the promo hasn't been shown too many times.
        int showCount = prefs.getInteger(FINDS_OPT_IN_PROMO_SHOWN_COUNT);
        int maxShowCount = FindsFeatures.sMaxOptInPromoInteractionCount.getValue();
        if (showCount >= maxShowCount) {
            return false;
        }

        // Check that the promo is not under cooldown, otherwise do not show.
        long lastShown = prefs.getLong(FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP);
        long daysSinceLastShown =
                TimeUnit.MILLISECONDS.toDays(System.currentTimeMillis() - lastShown);
        int cooldownDays = FindsFeatures.sOptInPromoCooldownDays.getValue();
        return daysSinceLastShown >= cooldownDays;
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

    /**
     * Accepts the Finds opt-in promo, initializing the channel and launching settings or showing a
     * snackbar as appropriate.
     *
     * @param context The current context.
     * @param profile The current user profile.
     * @param snackbarManager The SnackbarManager to show the snackbar.
     * @param onDismiss Runnable to call to dismiss the UI.
     */
    public static void acceptOptIn(
            Context context, Profile profile, SnackbarManager snackbarManager, Runnable onDismiss) {
        isFindsChannelCreated(
                (channelExists) -> {
                    boolean firstTime = !channelExists;
                    if (firstTime) {
                        // For first time opt-in, initialize the notification channel as enabled.
                        new ChannelsInitializer(
                                        BaseNotificationManagerProxyFactory.create(),
                                        ChromeChannelDefinitions.getInstance(),
                                        context.getResources())
                                .ensureInitialized(ChannelId.CHROME_FINDS);
                    }

                    // Verify if notifications are fully enabled (both app-level and channel).
                    // If not, redirect to settings so the user can finalize their opt-in.
                    // This handles first-time users with app-level disabled and testing
                    // configuration flow where app-level/channel notifications aren't enabled.
                    areFindsNotificationsEnabled(
                            (enabled) -> {
                                onDismiss.run();

                                if (!enabled) {
                                    launchFindsNotificationSettings(context);
                                } else {
                                    showOptInSnackbar(context, snackbarManager);
                                }

                                FindsMetrics.recordOptInAccepted(firstTime);
                                setOptInPromoInteracted(profile);
                            });
                });
    }

    /**
     * Shows a snackbar after the user opts in to the Finds promo.
     *
     * @param context The current context.
     * @param snackbarManager The SnackbarManager to show the snackbar.
     */
    private static void showOptInSnackbar(Context context, SnackbarManager snackbarManager) {
        snackbarManager.showSnackbar(
                Snackbar.make(
                                context.getString(R.string.chrome_finds_opt_in_snackbar_message),
                                new SnackbarController() {
                                    @Override
                                    public void onAction(@Nullable Object actionData) {
                                        launchFindsNotificationSettings(context);
                                        FindsMetrics.recordSnackbarActionClicked();
                                    }
                                },
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_CHROME_FINDS_OPT_IN)
                        .setAction(
                                context.getString(
                                        R.string.chrome_finds_opt_in_snackbar_action_text),
                                null));
    }
}
