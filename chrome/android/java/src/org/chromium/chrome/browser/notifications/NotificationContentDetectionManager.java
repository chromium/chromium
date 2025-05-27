// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
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
import java.util.Optional;

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
        public final String scopeUrl;
        public final String profileId;
        public final boolean incognito;
        public final String webApkPackage;

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
                String scopeUrl,
                String profileId,
                boolean incognito,
                String webApkPackage) {
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
            this.scopeUrl = scopeUrl;
            this.profileId = profileId;
            this.incognito = incognito;
            this.webApkPackage = webApkPackage;
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
                                    ChromeFeatureList.sReportNotificationContentDetectionData
                                                    .isEnabled()
                                            ? R.drawable.ic_warning_red_24dp
                                            : R.drawable.report_octagon)
                            .setTicker(tickerText)
                            .setTimestamp(timestamp)
                            .setRenotify(false)
                            .setOrigin(
                                    UrlFormatter.formatUrlForSecurityDisplay(
                                            origin, SchemeDisplay.OMIT_HTTP_AND_HTTPS));
            if (ChromeFeatureList.sReportNotificationContentDetectionData.isEnabled()) {
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

            // Store original notification contents and metadata as extras.
            Bundle originalNotificationBackup = new Bundle();
            originalNotificationBackup.putParcelable(
                    NotificationConstants.EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT,
                    originalNotification);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_ID, notificationId);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, notificationOrigin);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE, scopeUrl);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, profileId);
            originalNotificationBackup.putBoolean(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO, incognito);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE, webApkPackage);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_CHANNEL_ID, channelId);
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
                            /* notificationId= */ NotificationPlatformBridge.PLATFORM_ID));
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

    /** Called when a notification is suspicious and a warning should be shown in its place. */
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
            String scopeUrl,
            String profileId,
            boolean incognito,
            String webApkPackage) {
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
                        scopeUrl,
                        profileId,
                        incognito,
                        webApkPackage);
        warningNotificationAttributes.showWarning();
        recordSuspiciousNotificationWarningInteractions(
                SuspiciousNotificationWarningInteractions.WARNING_SHOWN);
    }

    static boolean shouldAllowReportingSpam(Bundle extras) {
        if (ChromeFeatureList.sReportNotificationContentDetectionData.isEnabled()
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

    /**
     * Called when the user clicks the `ACTION_SHOW_ORIGINAL_NOTIFICATION` button, expressly
     * dismisses the suspicious warning notification, and then shows the original notification with
     * the `ACTION_ALWAYS_ALLOW` button.
     */
    public static void showOriginalNotification(String warningNotificationId) {
        Context context = ContextUtils.getApplicationContext();
        var notificationManager = BaseNotificationManagerProxyFactory.create();
        notificationManager.getActiveNotifications(
                (activeNotifications) -> {
                    // Find extras for the warning notification with id, `warningNotificationId`.
                    Bundle warningNotificationExtras =
                            NotificationPlatformBridge.findNotificationExtras(
                                    activeNotifications, warningNotificationId);

                    // Obtain the backup notification from the extras found above.
                    Optional<Notification> notificationBackupOptional =
                            NotificationPlatformBridge.getNotificationBackupOrCancel(
                                    warningNotificationExtras,
                                    warningNotificationId,
                                    NotificationConstants
                                            .EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT);

                    // If a backup is found, use it to display the notification silently using the
                    // other fields stored in the extras.
                    if (notificationBackupOptional.isPresent()) {
                        Notification notificationBackup = notificationBackupOptional.get();
                        Notification.Builder builder =
                                Notification.Builder.recoverBuilder(context, notificationBackup);

                        // Store original notification contents as an extra in order to restore the
                        // original notification without "Always allow" button and also any other
                        // notifications that has "Always allow" button.
                        Bundle extras = new Bundle();
                        extras.putParcelable(
                                NotificationConstants
                                        .EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT,
                                notificationBackup.clone());
                        // Store extra signaling that the user has seen the original notification.
                        // This will help determine whether to allow the reporting feature from the
                        // unsubscribe confirmation notification or not.
                        extras.putBoolean(
                                NotificationConstants
                                        .EXTRA_ALLOW_REPORTING_AS_SPAM_IS_NOTIFICATION_WARNED,
                                true);
                        builder.addExtras(extras);

                        // Get notification attributes from Bundle.
                        String notificationId =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants.EXTRA_NOTIFICATION_ID);
                        String notificationOrigin =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN);
                        String scopeUrl =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE);
                        String profileId =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID);
                        boolean incognito =
                                getBooleanFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants
                                                .EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO,
                                        /* defaultValue= */ false);
                        String webApkPackage =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants
                                                .EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE);
                        String channelId =
                                getStringFromBackupBundle(
                                        warningNotificationExtras,
                                        NotificationConstants.EXTRA_NOTIFICATION_INFO_CHANNEL_ID);

                        // Add the unsubscribe and always allow notification buttons. If the feature
                        // parameter specifies to swap buttons, then "Unsubscribe" should be the
                        // secondary button. Otherwise, it should be the primary button.
                        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS,
                                SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME,
                                /* defaultValue= */ false)) {
                            appendAlwaysAllowButton(
                                    builder,
                                    notificationId,
                                    notificationOrigin,
                                    scopeUrl,
                                    profileId,
                                    incognito,
                                    webApkPackage,
                                    channelId);
                            appendUnsubscribeButton(
                                    builder,
                                    notificationId,
                                    notificationOrigin,
                                    scopeUrl,
                                    profileId,
                                    incognito,
                                    webApkPackage,
                                    channelId);
                        } else {
                            appendUnsubscribeButton(
                                    builder,
                                    notificationId,
                                    notificationOrigin,
                                    scopeUrl,
                                    profileId,
                                    incognito,
                                    webApkPackage,
                                    channelId);
                            appendAlwaysAllowButton(
                                    builder,
                                    notificationId,
                                    notificationOrigin,
                                    scopeUrl,
                                    profileId,
                                    incognito,
                                    webApkPackage,
                                    channelId);
                        }
                        NotificationPlatformBridge.displayNotificationSilently(
                                builder, notificationId);
                    }
                });

        recordSuspiciousNotificationWarningInteractions(
                SuspiciousNotificationWarningInteractions.SHOW_ORIGINAL_NOTIFICATION);
    }

    private static String getStringFromBackupBundle(Bundle notificationExtras, String extraType) {
        if (notificationExtras == null || !notificationExtras.containsKey(extraType)) {
            return "";
        }

        return (String) notificationExtras.getString(extraType);
    }

    private static boolean getBooleanFromBackupBundle(
            Bundle notificationExtras, String extraType, boolean defaultValue) {
        if (notificationExtras == null || !notificationExtras.containsKey(extraType)) {
            return defaultValue;
        }

        return (boolean) notificationExtras.getBoolean(extraType);
    }

    private static void appendUnsubscribeButton(
            Notification.Builder notificationBuilder,
            String id,
            String origin,
            String scopeUrl,
            String profileId,
            boolean incognito,
            String webApkPackage,
            String channelId) {
        PendingIntentProvider unsubscribePendingIntentProvider =
                makePendingIntentForButton(
                        id,
                        origin,
                        scopeUrl,
                        profileId,
                        incognito,
                        webApkPackage,
                        channelId,
                        NotificationConstants.ACTION_PRE_UNSUBSCRIBE);

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        notificationBuilder.addAction(
                new Notification.Action.Builder(
                                /* iconId= */ 0,
                                res.getString(R.string.notification_unsubscribe_button),
                                unsubscribePendingIntentProvider.getPendingIntent())
                        .build());
    }

    private static void appendAlwaysAllowButton(
            Notification.Builder notificationBuilder,
            String id,
            String origin,
            String scopeUrl,
            String profileId,
            boolean incognito,
            String webApkPackage,
            String channelId) {
        PendingIntentProvider alwaysAllowPendingIntentProvider =
                makePendingIntentForButton(
                        id,
                        origin,
                        scopeUrl,
                        profileId,
                        incognito,
                        webApkPackage,
                        channelId,
                        NotificationConstants.ACTION_ALWAYS_ALLOW);

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        notificationBuilder.addAction(
                new Notification.Action.Builder(
                                /* iconId= */ 0,
                                res.getString(R.string.notification_always_allow_button),
                                alwaysAllowPendingIntentProvider.getPendingIntent())
                        .build());
    }

    private static PendingIntentProvider makePendingIntentForButton(
            String id,
            String origin,
            String scopeUrl,
            String profileId,
            boolean incognito,
            String webApkPackage,
            String channelId,
            String action) {
        Context context = ContextUtils.getApplicationContext();
        Uri intentData =
                NotificationPlatformBridge.makeIntentData(id, origin, /* actionIndex= */ -1);
        // TODO(crbug.com/359909538): Telemetry shows that startService-type intents are even more
        // unreliable than broadcasts. Furthermore, checking the feature state is currently the only
        // place in this method that in theory requires native startup. In practice, we will only
        // ever get called with ACTION_PRE_UNSUBSCRIBE when displaying a web notification, which
        // implies native is running, making this a non-issue. Neverthelerss, removing support for
        // startService-type intents would be the cleanest solution here.
        boolean useServiceIntent =
                NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(action)
                        && NotificationIntentInterceptor
                                .shouldUseServiceIntentForPreUnsubscribeAction();
        Intent intent = new Intent(action, intentData);
        intent.setClass(
                context,
                useServiceIntent
                        ? NotificationService.class
                        : NotificationServiceImpl.Receiver.class);

        // Make sure to update NotificationJobService.getJobExtrasFromIntent() when changing any
        // of the extras included with the |intent|.
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, id);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_TYPE, NotificationType.WEB_PERSISTENT);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, origin);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE, scopeUrl);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, profileId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO, incognito);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE, webApkPackage);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_CHANNEL_ID, channelId);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_ACTION_INDEX, /* actionIndex= */ -1);

        // This flag ensures the broadcast is delivered with foreground priority. It also means the
        // receiver gets a shorter timeout interval before it may be killed, but this is ok because
        // we schedule a job to handle the intent in NotificationService.Receiver.
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);

        if (useServiceIntent) {
            return PendingIntentProvider.getService(
                    context,
                    NotificationPlatformBridge.PENDING_INTENT_REQUEST_CODE,
                    intent,
                    PendingIntent.FLAG_UPDATE_CURRENT,
                    /* mutable= */ false);
        }

        return PendingIntentProvider.getBroadcast(
                context,
                NotificationPlatformBridge.PENDING_INTENT_REQUEST_CODE,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT,
                /* mutable= */ false);
    }
}
