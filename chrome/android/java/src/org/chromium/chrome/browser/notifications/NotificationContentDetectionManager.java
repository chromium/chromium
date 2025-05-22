// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/** Helper utils for replacing suspicious notifications with warnings. */
public class NotificationContentDetectionManager {
    @VisibleForTesting
    public static final String SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME =
            "SafeBrowsing.SuspiciousNotificationWarning.Interactions";

    // The name of the feature parameter that when set to true, switches the order of buttons on a
    // notification when showing warnings.
    @VisibleForTesting
    static final String SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME =
            "ShowWarningsForSuspiciousNotificationsShouldSwapButtons";

    // Maps the origins of suspicious notifications and their ids, used for UMA logging.
    @VisibleForTesting
    public static final Map<String, HashSet<String>> sSuspiciousNotificationsMap = new HashMap();

    private static @Nullable NotificationContentDetectionManager sInstance;

    private final BaseNotificationManagerProxy mNotificationManager;

    /**
     * Encapsulates attributes that are necessary for creating a NotificationWrapper for the warning
     * notification.
     */
    private class WarningNotificationWrapperAttributes {
        public final String notificationId;
        public final String notificationOrigin;
        public final int vibrationPatternDefaults;
        public final long[] vibrationPattern;
        public final CharSequence tickerText;
        public final long timestamp;
        public final boolean silent;
        public final boolean shouldSetChannelId;
        public final String channelId;
        public final Notification originalNotification;
        public final PendingIntentProvider deleteIntentProvider;
        public final PendingIntentProvider showOriginalNotificationIntentProvider;
        public final PendingIntentProvider unsubscribeIntentProvider;
        public final int platformId;

        public WarningNotificationWrapperAttributes(
                String notificationId,
                String notificationOrigin,
                int vibrationPatternDefaults,
                long[] vibrationPattern,
                CharSequence tickerText,
                long timestamp,
                boolean silent,
                boolean shouldSetChannelId,
                String channelId,
                Notification originalNotification,
                PendingIntentProvider deleteIntentProvider,
                PendingIntentProvider showOriginalNotificationIntentProvider,
                PendingIntentProvider unsubscribeIntentProvider,
                int platformId) {
            this.notificationId = notificationId;
            this.notificationOrigin = notificationOrigin;
            this.vibrationPatternDefaults = vibrationPatternDefaults;
            this.vibrationPattern = vibrationPattern;
            this.tickerText = tickerText;
            this.timestamp = timestamp;
            this.silent = silent;
            this.shouldSetChannelId = shouldSetChannelId;
            this.channelId = channelId;
            this.originalNotification = originalNotification;
            this.deleteIntentProvider = deleteIntentProvider;
            this.showOriginalNotificationIntentProvider = showOriginalNotificationIntentProvider;
            this.unsubscribeIntentProvider = unsubscribeIntentProvider;
            this.platformId = platformId;
        }

        public void showWarning() {
            NotificationWrapper notificationWrapper = createWarningNotificationWrapper();
            mNotificationManager.notify(notificationWrapper);
        }

        /**
         * This method generates a custom warning notification, which should be displayed instead of
         * the original notification when the on-device model finds the original notification's
         * contents to be suspicious. The warning notification should have two possible actions:
         * unsubscribe from the site's notifications and show the original notification contents. To
         * preserve the contents of the original notification, in case the user decides they want to
         * see them, they are stored as an extra on the warning notification so they can be obtained
         * later.
         */
        private NotificationWrapper createWarningNotificationWrapper() {
            Context context = ContextUtils.getApplicationContext();
            Resources res = context.getResources();

            final String origin = notificationOrigin;
            NotificationBuilderBase notificationBuilder =
                    new StandardNotificationBuilder(context)
                            .setTitle(res.getString(R.string.notification_warning_title))
                            .setBody(
                                    res.getString(
                                            R.string.notification_warning_body,
                                            UrlFormatter.formatUrlForSecurityDisplay(
                                                    notificationOrigin,
                                                    SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                            .setSmallIconId(
                                    ChromeFeatureList.isEnabled(
                                                    ChromeFeatureList
                                                            .REPORT_NOTIFICATION_CONTENT_DETECTION_DATA)
                                            ? R.drawable.ic_warning_red_24dp
                                            : R.drawable.report_octagon)
                            .setTicker(tickerText)
                            .setTimestamp(timestamp)
                            .setRenotify(false)
                            .setOrigin(
                                    UrlFormatter.formatUrlForSecurityDisplay(
                                            origin, SchemeDisplay.OMIT_HTTP_AND_HTTPS));
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA)) {
                // Don't show default icon on warning notification.
                notificationBuilder.setSuppressShowingLargeIcon(true);
            }

            if (shouldSetChannelId) {
                // TODO(crbug.com/40544272): Channel ID should be retrieved from cache in native and
                // passed through to here with other notification parameters.
                notificationBuilder.setChannelId(channelId);
            }

            notificationBuilder.setDefaults(vibrationPatternDefaults);
            notificationBuilder.setVibrate(vibrationPattern);
            notificationBuilder.setSilent(silent);

            // Store original notification contents as an extra.
            Bundle originalNotificationBackup = new Bundle();
            originalNotificationBackup.putParcelable(
                    NotificationConstants.EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT,
                    originalNotification);
            notificationBuilder.setExtras(originalNotificationBackup);

            // Closing the notification should delete it.
            notificationBuilder.setDeleteIntent(deleteIntentProvider);

            // Add the unsubscribe and show original notification buttons. If the feature parameter
            // specifies to swap buttons, then "Unsubscribe" should be the secondary button.
            // Otherwise, it should be the primary button.
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS,
                    SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME,
                    /* defaultValue= */ false)) {
                notificationBuilder.addSettingsAction(
                        /* iconId= */ 0,
                        res.getString(R.string.notification_show_original_button),
                        showOriginalNotificationIntentProvider,
                        NotificationUmaTracker.ActionType.SHOW_ORIGINAL_NOTIFICATION);
                notificationBuilder.addSettingsAction(
                        /* iconId= */ 0,
                        res.getString(R.string.notification_unsubscribe_button),
                        unsubscribeIntentProvider,
                        NotificationUmaTracker.ActionType.PRE_UNSUBSCRIBE);
            } else {
                notificationBuilder.addSettingsAction(
                        /* iconId= */ 0,
                        res.getString(R.string.notification_unsubscribe_button),
                        unsubscribeIntentProvider,
                        NotificationUmaTracker.ActionType.PRE_UNSUBSCRIBE);
                notificationBuilder.addSettingsAction(
                        /* iconId= */ 0,
                        res.getString(R.string.notification_show_original_button),
                        showOriginalNotificationIntentProvider,
                        NotificationUmaTracker.ActionType.SHOW_ORIGINAL_NOTIFICATION);
            }

            // Add entry to `sSuspiciousNotificationsMap` for UMA logging.
            if (sSuspiciousNotificationsMap.containsKey(notificationOrigin)) {
                sSuspiciousNotificationsMap.get(notificationOrigin).add(notificationId);
            } else {
                HashSet<String> suspiciousNotificationIds = new HashSet<>();
                suspiciousNotificationIds.add(notificationId);
                sSuspiciousNotificationsMap.put(notificationOrigin, suspiciousNotificationIds);
            }

            return notificationBuilder.build(
                    new NotificationMetadata(
                            NotificationUmaTracker.SystemNotificationType.SITES,
                            /* notificationTag= */ notificationId,
                            /* notificationId= */ platformId));
        }
    }

    /**
     * All user interactions that can happen when a warning is shown for a suspicious notification.
     * Must be kept in sync with SuspiciousNotificationWarningInteractions in
     * safe_browsing/enums.xml.
     */
    @IntDef({
        SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
        SuspiciousNotificationWarningInteractions.SHOW_ORIGINAL_NOTIFICATION,
        SuspiciousNotificationWarningInteractions.UNSUBSCRIBE,
        SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW,
        SuspiciousNotificationWarningInteractions.DISMISS,
        SuspiciousNotificationWarningInteractions.REPORT_AS_SAFE,
        SuspiciousNotificationWarningInteractions.REPORT_WARNED_NOTIFICATION_AS_SPAM,
        SuspiciousNotificationWarningInteractions.REPORT_UNWARNED_NOTIFICATION_AS_SPAM,
        SuspiciousNotificationWarningInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuspiciousNotificationWarningInteractions {
        int WARNING_SHOWN = 0;
        int SHOW_ORIGINAL_NOTIFICATION = 1;
        int UNSUBSCRIBE = 2;
        int ALWAYS_ALLOW = 3;
        int DISMISS = 4;
        int REPORT_AS_SAFE = 5;
        int REPORT_WARNED_NOTIFICATION_AS_SPAM = 6;
        int REPORT_UNWARNED_NOTIFICATION_AS_SPAM = 7;
        int MAX_VALUE = REPORT_UNWARNED_NOTIFICATION_AS_SPAM;
    }

    /** Returns the singleton WebappRegistry instance. Creates the instance on first call. */
    public static NotificationContentDetectionManager create(
            BaseNotificationManagerProxy notificationManager) {
        if (sInstance == null) {
            sInstance = new NotificationContentDetectionManager(notificationManager);
        }
        return sInstance;
    }

    private NotificationContentDetectionManager(BaseNotificationManagerProxy notificationManager) {
        mNotificationManager = notificationManager;
    }

    public static boolean isNotificationSuspicious(String notificationId, String origin) {
        if (sSuspiciousNotificationsMap.containsKey(origin)) {
            return sSuspiciousNotificationsMap.get(origin).contains(notificationId);
        }
        return false;
    }

    static void recordSuspiciousNotificationWarningInteractions(
            @SuspiciousNotificationWarningInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                value,
                SuspiciousNotificationWarningInteractions.MAX_VALUE);
    }

    /** Logs the `interaction` for UMA if the `notificationId` from `origin` is suspicious. */
    public static void recordInteractionForUMAIfSuspicious(
            String origin,
            String notificationId,
            @SuspiciousNotificationWarningInteractions int interaction) {
        // If the origin is not suspicious, nothing to record.
        if (!sSuspiciousNotificationsMap.containsKey(origin)) {
            return;
        }

        switch (interaction) {
            case SuspiciousNotificationWarningInteractions.DISMISS:
                // Record only if the notification is suspicious and remove the `notificationId` so
                // that notification is not recorded again.
                if (isNotificationSuspicious(notificationId, origin)) {
                    sSuspiciousNotificationsMap.get(origin).remove(notificationId);
                    recordSuspiciousNotificationWarningInteractions(interaction);
                }
                return;
            case SuspiciousNotificationWarningInteractions.UNSUBSCRIBE:
            case SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW:
                // Record only if triggered from a suspicious notification. Remove the `origin`
                // entry regardless, since future notifications from this origin should no longer be
                // recorded.
                if (isNotificationSuspicious(notificationId, origin)) {
                    recordSuspiciousNotificationWarningInteractions(interaction);
                }
                sSuspiciousNotificationsMap.remove(origin);
                return;
        }
    }

    public void showWarning(
            String notificationId,
            String notificationOrigin,
            int vibrationPatternDefaults,
            long[] vibrationPattern,
            CharSequence tickerText,
            long timestamp,
            boolean silent,
            boolean shouldSetChannelId,
            String channelId,
            Notification originalNotification,
            PendingIntentProvider deleteIntentProvider,
            PendingIntentProvider showOriginalNotificationIntentProvider,
            PendingIntentProvider unsubscribeIntentProvider,
            int platformId,
            Profile profile) {
        WarningNotificationWrapperAttributes warningNotificationAttributes =
                new WarningNotificationWrapperAttributes(
                        notificationId,
                        notificationOrigin,
                        vibrationPatternDefaults,
                        vibrationPattern,
                        tickerText,
                        timestamp,
                        silent,
                        shouldSetChannelId,
                        channelId,
                        originalNotification,
                        deleteIntentProvider,
                        showOriginalNotificationIntentProvider,
                        unsubscribeIntentProvider,
                        platformId);
        warningNotificationAttributes.showWarning();
        recordSuspiciousNotificationWarningInteractions(
                SuspiciousNotificationWarningInteractions.WARNING_SHOWN);
    }

    static boolean shouldAllowReportingSpam(Bundle extras) {
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA)
                && extras.containsKey(
                        NotificationConstants
                                .EXTRA_ALLOW_REPORTING_AS_SPAM_IS_NOTIFICATION_WARNED)) {
            return true;
        }
        return false;
    }

    static boolean wasNotificationWarned(Bundle extras) {
        return extras.getBoolean(
                NotificationConstants.EXTRA_ALLOW_REPORTING_AS_SPAM_IS_NOTIFICATION_WARNED);
    }
}
