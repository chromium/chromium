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
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy.StatusBarNotificationProxy;
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

/** Helper utils for replacing suspicious notifications with warnings. */
@NullMarked
public class NotificationContentDetectionManager {
    @VisibleForTesting
    static final String SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME =
            "SafeBrowsing.SuspiciousNotificationWarning.Interactions";

    // The name of the feature parameter that when set to true, switches the order of buttons on a
    // notification when showing warnings.
    @VisibleForTesting
    static final String SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME =
            "ShowWarningsForSuspiciousNotificationsShouldSwapButtons";

    // Maps the origins of suspicious notifications and their ids, used for UMA logging.
    @VisibleForTesting
    static final Map<String, HashSet<String>> sSuspiciousNotificationsMap = new HashMap();

    // Maps "always allowed" origins to the notification id where the "Always allow" button was
    // tapped. Used for reporting notifications to Google upon user consent.
    static final Map<String, String> sAlwaysAllowNotificationsMap = new HashMap<>();

    // Maps origins of suspicious notifications to details that require persisting for possibly
    // showing suspended suspicious notifications to the user at their request. This also keeps
    // track of the warning notification id and the warning count for the origin, which should be
    // displayed on the warning notification since there should only be 1 warning notification per
    // origin. If this data is pruned from memory (e.g., Chrome is killed), the user will not be
    // able to restore the original notifications by tapping "Show original notification" on the
    // warning notification.
    static final Map<String, SuspiciousNotificationWarningDetailsForOrigin>
            sWarningNotificationAttributesByOrigin = new HashMap<>();

    private static @Nullable NotificationContentDetectionManager sInstance;

    private final BaseNotificationManagerProxy mNotificationManager;

    /**
     * Encapsulates attributes that are necessary for creating a NotificationWrapper for the warning
     * notification.
     */
    private class WarningNotificationWrapperAttributes {
        private final String mNotificationId;
        private final String mNotificationOrigin;
        private final boolean mVibrateEnabled;
        private final int[] mVibrationPattern;
        private final long mTimestamp;
        private final boolean mSilent;
        private final @Nullable String mChannelId;
        private final Notification mOriginalNotification;
        private final String mScopeUrl;
        private final @Nullable String mProfileId;
        private final boolean mIncognito;
        private final String mWebApkPackage;

        WarningNotificationWrapperAttributes(
                String notificationId,
                String notificationOrigin,
                boolean vibrateEnabled,
                int[] vibrationPattern,
                long timestamp,
                boolean silent,
                @Nullable String channelId,
                Notification originalNotification,
                String scopeUrl,
                @Nullable String profileId,
                boolean incognito,
                String webApkPackage) {
            this.mNotificationId = notificationId;
            this.mNotificationOrigin = notificationOrigin;
            this.mVibrateEnabled = vibrateEnabled;
            this.mVibrationPattern = vibrationPattern;
            this.mTimestamp = timestamp;
            this.mSilent = silent;
            this.mChannelId = channelId;
            this.mOriginalNotification = originalNotification;
            this.mScopeUrl = scopeUrl;
            this.mProfileId = profileId;
            this.mIncognito = incognito;
            this.mWebApkPackage = webApkPackage;
        }

        void showWarning(int warningCount) {
            NotificationWrapper notificationWrapper =
                    createWarningNotificationWrapper(warningCount);
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
        private NotificationWrapper createWarningNotificationWrapper(int warningCount) {
            Context context = ContextUtils.getApplicationContext();
            Resources res = context.getResources();

            NotificationBuilderBase notificationBuilder =
                    new StandardNotificationBuilder(context)
                            .setTitle(
                                    res.getQuantityString(
                                            R.plurals.notification_warning_title,
                                            warningCount,
                                            warningCount))
                            .setBody(
                                    res.getString(
                                            R.string.notification_warning_body,
                                            UrlFormatter.formatUrlForSecurityDisplay(
                                                    mNotificationOrigin,
                                                    SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                            .setSmallIconId(
                                    ChromeFeatureList.sReportNotificationContentDetectionData
                                                    .isEnabled()
                                            ? R.drawable.ic_warning_red_24dp
                                            : R.drawable.report_octagon)
                            .setTicker(
                                    NotificationPlatformBridge.createTickerText(
                                            res.getQuantityString(
                                                    R.plurals.notification_warning_title,
                                                    warningCount,
                                                    warningCount),
                                            res.getString(
                                                    R.string.notification_warning_body,
                                                    UrlFormatter.formatUrlForSecurityDisplay(
                                                            mNotificationOrigin,
                                                            SchemeDisplay.OMIT_HTTP_AND_HTTPS))))
                            .setTimestamp(mTimestamp)
                            .setRenotify(false)
                            .setOrigin(
                                    UrlFormatter.formatUrlForSecurityDisplay(
                                            mNotificationOrigin,
                                            SchemeDisplay.OMIT_HTTP_AND_HTTPS));
            if (ChromeFeatureList.sReportNotificationContentDetectionData.isEnabled()) {
                // Don't show default icon on warning notification.
                notificationBuilder.setSuppressShowingLargeIcon(true);
            }

            // When a notification is part of a WebAPK, the channel id is managed by the WebAPK
            // and should not be set here.
            if (mWebApkPackage.isEmpty()) {
                // TODO(crbug.com/40544272): Channel ID should be retrieved from cache in native and
                // passed through to here with other notification parameters.
                notificationBuilder.setChannelId(mChannelId);
            }

            notificationBuilder.setDefaults(
                    NotificationPlatformBridge.makeDefaults(
                            mVibrateEnabled
                                    ? mVibrationPattern.length
                                    : NotificationPlatformBridge.EMPTY_VIBRATION_PATTERN.length,
                            mSilent,
                            mVibrateEnabled));
            notificationBuilder.setVibrate(
                    NotificationPlatformBridge.makeVibrationPattern(
                            mVibrateEnabled
                                    ? mVibrationPattern
                                    : NotificationPlatformBridge.EMPTY_VIBRATION_PATTERN));
            notificationBuilder.setSilent(mSilent);

            // Store original notification contents and metadata as extras.
            Bundle originalNotificationBackup = new Bundle();
            originalNotificationBackup.putParcelable(
                    NotificationConstants.EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT,
                    mOriginalNotification);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_ID, mNotificationId);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, mNotificationOrigin);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE, mScopeUrl);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, mProfileId);
            originalNotificationBackup.putBoolean(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO, mIncognito);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE, mWebApkPackage);
            originalNotificationBackup.putString(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_CHANNEL_ID, mChannelId);
            originalNotificationBackup.putInt(
                    NotificationConstants.EXTRA_SUSPICIOUS_NOTIFICATION_COUNT, warningCount);
            notificationBuilder.setExtras(originalNotificationBackup);

            // Closing the notification should delete it.
            notificationBuilder.setDeleteIntent(
                    makePendingIntentForButton(
                            mNotificationId,
                            mNotificationOrigin,
                            mScopeUrl,
                            mProfileId,
                            mIncognito,
                            mWebApkPackage,
                            mChannelId,
                            NotificationConstants.ACTION_CLOSE_NOTIFICATION));

            // Add the unsubscribe and show original notification buttons. If the feature parameter
            // specifies to swap buttons, then "Unsubscribe" should be the secondary button.
            // Otherwise, it should be the primary button.
            PendingIntentProvider showOriginalNotificationIntentProvider =
                    makePendingIntentForButton(
                            mNotificationId,
                            mNotificationOrigin,
                            mScopeUrl,
                            mProfileId,
                            mIncognito,
                            mWebApkPackage,
                            mChannelId,
                            NotificationConstants.ACTION_SHOW_ORIGINAL_NOTIFICATION);
            PendingIntentProvider unsubscribeIntentProvider =
                    makePendingIntentForButton(
                            mNotificationId,
                            mNotificationOrigin,
                            mScopeUrl,
                            mProfileId,
                            mIncognito,
                            mWebApkPackage,
                            mChannelId,
                            NotificationConstants.ACTION_PRE_UNSUBSCRIBE);
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS,
                    SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME,
                    /* defaultValue= */ false)) {
                notificationBuilder.addSettingsAction(
                        /* iconId= */ 0,
                        res.getQuantityString(
                                R.plurals.notification_show_original_button,
                                warningCount,
                                warningCount),
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
                        res.getQuantityString(
                                R.plurals.notification_show_original_button,
                                warningCount,
                                warningCount),
                        showOriginalNotificationIntentProvider,
                        NotificationUmaTracker.ActionType.SHOW_ORIGINAL_NOTIFICATION);
            }

            return notificationBuilder.build(
                    new NotificationMetadata(
                            NotificationUmaTracker.SystemNotificationType.SITES,
                            /* notificationTag= */ mNotificationId,
                            /* notificationId= */ NotificationPlatformBridge.PLATFORM_ID));
        }
    }

    /**
     * Stores pertinent information about a suspicious notification warning and the original
     * notification backups for a specific origin.
     */
    private static class SuspiciousNotificationWarningDetailsForOrigin {
        // Notification id of the warning shown to the user for this origin.
        final String mWarningNotificationId;
        // Contents of suspicious notifications from the same origin.
        private final Map<String, WarningNotificationWrapperAttributes>
                mOriginalNotificationBackups;

        SuspiciousNotificationWarningDetailsForOrigin(
                String warningNotificationId,
                Map<String, WarningNotificationWrapperAttributes> mOriginalNotificationBackups) {
            this.mWarningNotificationId = warningNotificationId;
            this.mOriginalNotificationBackups = mOriginalNotificationBackups;
        }

        void updateBackupsAndShowWarning(
                String notificationId,
                WarningNotificationWrapperAttributes warningNotificationAttributes) {
            mOriginalNotificationBackups.put(notificationId, warningNotificationAttributes);
            if (mOriginalNotificationBackups.containsKey(mWarningNotificationId)) {
                mOriginalNotificationBackups
                        .get(mWarningNotificationId)
                        .showWarning(mOriginalNotificationBackups.size());
                // If the warning notification to be shown is `notificationId`, then a new warning
                // is shown. Otherwise, the existing one is being updated.
                if (notificationId.equals(mWarningNotificationId)) {
                    recordSuspiciousNotificationWarningInteractions(
                            SuspiciousNotificationWarningInteractions.WARNING_SHOWN);
                } else {
                    recordSuspiciousNotificationWarningInteractions(
                            SuspiciousNotificationWarningInteractions.SUPPRESS_DUPLICATE_WARNING);
                }
            }
        }

        int displayOriginalNotificationsFromBackups() {
            int actualNumSuspiciousBackupsDisplayed = 0;
            for (String notificationId : mOriginalNotificationBackups.keySet()) {
                // Skip the `mWarningNotificationId` because this will be displayed using the
                // warning notification's extra Bundle.
                if (notificationId.equals(mWarningNotificationId)) {
                    continue;
                }
                WarningNotificationWrapperAttributes notificationAttributes =
                        mOriginalNotificationBackups.get(notificationId);
                displayOriginalNotification(
                        notificationAttributes.mOriginalNotification,
                        notificationId,
                        notificationAttributes.mNotificationOrigin,
                        notificationAttributes.mScopeUrl,
                        notificationAttributes.mProfileId,
                        notificationAttributes.mIncognito,
                        notificationAttributes.mWebApkPackage,
                        notificationAttributes.mChannelId);
                actualNumSuspiciousBackupsDisplayed += 1;
            }
            return actualNumSuspiciousBackupsDisplayed;
        }
    }

    /**
     * All user interactions that can happen when a warning is shown for a suspicious notification.
     * Must be kept in sync with SuspiciousNotificationWarningInteractions in
     * safe_browsing/enums.xml and
     * safe_browsing/android/notification_content_detection_manager_android.h.
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
        SuspiciousNotificationWarningInteractions.SUPPRESS_DUPLICATE_WARNING,
        SuspiciousNotificationWarningInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SuspiciousNotificationWarningInteractions {
        int WARNING_SHOWN = 0;
        int SHOW_ORIGINAL_NOTIFICATION = 1;
        int UNSUBSCRIBE = 2;
        int ALWAYS_ALLOW = 3;
        int DISMISS = 4;
        int REPORT_AS_SAFE = 5;
        int REPORT_WARNED_NOTIFICATION_AS_SPAM = 6;
        int REPORT_UNWARNED_NOTIFICATION_AS_SPAM = 7;
        int SUPPRESS_DUPLICATE_WARNING = 8;
        int MAX_VALUE = SUPPRESS_DUPLICATE_WARNING;
    }

    /** Returns the singleton WebappRegistry instance. Creates the instance on first call. */
    static NotificationContentDetectionManager create(
            BaseNotificationManagerProxy notificationManager) {
        if (sInstance == null) {
            sInstance = new NotificationContentDetectionManager(notificationManager);
        }
        return sInstance;
    }

    /** Called when a notification is suspicious and a warning should be shown in its place. */
    void showWarning(
            String notificationId,
            String notificationOrigin,
            boolean vibrateEnabled,
            int[] vibrationPattern,
            long timestamp,
            boolean silent,
            @Nullable String channelId,
            Notification originalNotification,
            String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            String webApkPackage) {
        WarningNotificationWrapperAttributes warningNotificationAttributes =
                new WarningNotificationWrapperAttributes(
                        notificationId,
                        notificationOrigin,
                        vibrateEnabled,
                        vibrationPattern,
                        timestamp,
                        silent,
                        channelId,
                        originalNotification,
                        scopeUrl,
                        profileId,
                        incognito,
                        webApkPackage);
        // Add entry to `sSuspiciousNotificationsMap` for UMA logging.
        if (sSuspiciousNotificationsMap.containsKey(notificationOrigin)) {
            sSuspiciousNotificationsMap.get(notificationOrigin).add(notificationId);
        } else {
            HashSet<String> suspiciousNotificationIds = new HashSet<>();
            suspiciousNotificationIds.add(notificationId);
            sSuspiciousNotificationsMap.put(notificationOrigin, suspiciousNotificationIds);
        }

        // Update warning notification count and persisted suspicious notifications for possible
        // displaying later.
        if (!sWarningNotificationAttributesByOrigin.containsKey(notificationOrigin)) {
            sWarningNotificationAttributesByOrigin.put(
                    notificationOrigin,
                    new SuspiciousNotificationWarningDetailsForOrigin(
                            /* mWarningNotificationId */ notificationId,
                            new HashMap<String, WarningNotificationWrapperAttributes>()));
        }
        sWarningNotificationAttributesByOrigin
                .get(notificationOrigin)
                .updateBackupsAndShowWarning(notificationId, warningNotificationAttributes);
    }

    /**
     * Called when the user clicks the `ACTION_ALWAYS_ALLOW` button, dismisses all active
     * notifications from the same origin and restores them to their original notifications in
     * receiving order. Done pre-native to ensure the confirmation notification is displayed after
     * active notifications are handled.
     */
    static void onNotificationPreAlwaysAllow(
            String notificationId,
            String origin,
            String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            String webApkPackage,
            @Nullable String channelId) {
        // Record "Always allow" action.
        recordInteractionForUMAIfSuspicious(
                origin, notificationId, SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW);
        // Remove the `origin` entry since future notifications from this origin no longer need to
        // be tracked or logged.
        sSuspiciousNotificationsMap.remove(origin);

        // Add entry to `sAlwaysAllowNotificationsMap` for possible reporting later.
        sAlwaysAllowNotificationsMap.put(origin, notificationId);

        // Update all currently active notifications from `origin` so that their original contents
        // and buttons are displayed. This also ensures these notifications no longer have the
        // "Always allow" button.
        Context context = ContextUtils.getApplicationContext();
        var notificationManager = BaseNotificationManagerProxyFactory.create();
        notificationManager.getActiveNotifications(
                (activeNotifications) -> {
                    for (StatusBarNotificationProxy proxy : activeNotifications) {
                        if (proxy.getId() != NotificationPlatformBridge.PLATFORM_ID
                                || !origin.equals(
                                        NotificationPlatformBridge.getOriginFromNotificationTag(
                                                proxy.getTag()))) {
                            continue;
                        }

                        Notification notificationBackup =
                                NotificationPlatformBridge.getNotificationBackupOrCancel(
                                        proxy.getNotification().extras,
                                        proxy.getTag(),
                                        NotificationConstants
                                                .EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT);

                        if (notificationBackup != null) {
                            Notification.Builder builder =
                                    Notification.Builder.recoverBuilder(
                                            context, notificationBackup);
                            appendUnsubscribeButton(
                                    builder,
                                    notificationId,
                                    origin,
                                    scopeUrl,
                                    profileId,
                                    incognito,
                                    webApkPackage,
                                    channelId);
                            NotificationPlatformBridge.displayNotificationSilently(
                                    builder, proxy.getTag());
                        }
                    }
                });
    }

    static void dismissNotification(String origin, String notificationId) {
        // Remove the notification's entry from `sSuspiciousNotificationsMap`. Since warnings from
        // the same origin are collapsed into one notification, then the entire entry in
        // `sSuspiciousNotificationsMap` can be removed.
        if (recordInteractionForUMAIfSuspicious(
                origin, notificationId, SuspiciousNotificationWarningInteractions.DISMISS)) {
            sSuspiciousNotificationsMap.remove(origin);
        }
        if (sWarningNotificationAttributesByOrigin.containsKey(origin)
                && sWarningNotificationAttributesByOrigin
                        .get(origin)
                        .mWarningNotificationId
                        .equals(notificationId)) {
            sWarningNotificationAttributesByOrigin.remove(origin);
        }
    }

    static void onPreUnsubscribeMaybeCommittedAfterWarning(String notificationId, String origin) {
        // Record when the unsubscribe is completed.
        recordInteractionForUMAIfSuspicious(
                origin, notificationId, SuspiciousNotificationWarningInteractions.UNSUBSCRIBE);
    }

    static void removeOriginFromSuspiciousMap(String origin) {
        sSuspiciousNotificationsMap.remove(origin);
        sWarningNotificationAttributesByOrigin.remove(origin);
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
    static void showOriginalNotifications(
            String warningNotificationId, String warningNotificationOrigin) {
        var notificationManager = BaseNotificationManagerProxyFactory.create();
        notificationManager.getActiveNotifications(
                (activeNotifications) -> {
                    // Find extras for the warning notification with id, `warningNotificationId`.
                    Bundle warningNotificationExtras =
                            NotificationPlatformBridge.findNotificationExtras(
                                    activeNotifications, warningNotificationId);

                    // Obtain the backup notification from the extras found above.
                    Notification notificationBackup =
                            NotificationPlatformBridge.getNotificationBackupOrCancel(
                                    warningNotificationExtras,
                                    warningNotificationId,
                                    NotificationConstants
                                            .EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT);

                    // If a backup is found, use it to display the notification silently using the
                    // other fields stored in the extras.
                    if (notificationBackup != null) {
                        // Get notification attributes from Bundle.
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

                        int expectedNumSuspiciousNotificationsLeft = 1;
                        if (warningNotificationExtras != null
                                && warningNotificationExtras.containsKey(
                                        NotificationConstants
                                                .EXTRA_SUSPICIOUS_NOTIFICATION_COUNT)) {
                            expectedNumSuspiciousNotificationsLeft =
                                    warningNotificationExtras.getInt(
                                            NotificationConstants
                                                    .EXTRA_SUSPICIOUS_NOTIFICATION_COUNT);
                        }

                        // Display the original notification, whose backup is stored in the warning
                        // notification's backup `Bundle`.
                        displayOriginalNotification(
                                notificationBackup,
                                warningNotificationId,
                                warningNotificationOrigin,
                                scopeUrl,
                                profileId,
                                incognito,
                                webApkPackage,
                                channelId);

                        // Record the number of suppressed suspicious notifications that the warning
                        // displays to the user, then subtract 1 from
                        // `expectedNumSuspiciousNotificationsLeft` because the first suspicious
                        // notification is displayed using the backup stored in the `Bundle`.
                        NotificationUmaTracker.recordSuspiciousNotificationCountOnShowOriginals(
                                expectedNumSuspiciousNotificationsLeft);
                        expectedNumSuspiciousNotificationsLeft -= 1;

                        // Display the rest of the original notifications and record the number of
                        // suspicious notifications that the user expected to be delivered but were
                        // not.
                        if (sWarningNotificationAttributesByOrigin.containsKey(
                                warningNotificationOrigin)) {
                            int actualNumSuspiciousBackupsDisplayed =
                                    sWarningNotificationAttributesByOrigin
                                            .get(warningNotificationOrigin)
                                            .displayOriginalNotificationsFromBackups();
                            NotificationUmaTracker
                                    .recordSuspiciousNotificationsDroppedCountOnShowOriginals(
                                            expectedNumSuspiciousNotificationsLeft
                                                    - actualNumSuspiciousBackupsDisplayed);
                            sWarningNotificationAttributesByOrigin.remove(
                                    warningNotificationOrigin);
                        } else {
                            NotificationUmaTracker
                                    .recordSuspiciousNotificationsDroppedCountOnShowOriginals(
                                            expectedNumSuspiciousNotificationsLeft);
                        }
                    }
                });

        recordSuspiciousNotificationWarningInteractions(
                SuspiciousNotificationWarningInteractions.SHOW_ORIGINAL_NOTIFICATION);
    }

    static void reportNotification(
            String notificationId,
            @SuspiciousNotificationWarningInteractions int reportingInteractionType) {
        // Cancel notification immediately so that the user perceives the action to have
        // been recognized; but return `true` as we still need native processing later to
        // actually revoke the permission.
        var notificationManager = BaseNotificationManagerProxyFactory.create();
        notificationManager.cancel(notificationId, NotificationPlatformBridge.PLATFORM_ID);
        recordSuspiciousNotificationWarningInteractions(reportingInteractionType);
    }

    private NotificationContentDetectionManager(BaseNotificationManagerProxy notificationManager) {
        mNotificationManager = notificationManager;
    }

    static boolean isNotificationSuspicious(String notificationId, String origin) {
        if (sSuspiciousNotificationsMap.containsKey(origin)) {
            return sSuspiciousNotificationsMap.get(origin).contains(notificationId);
        }
        return false;
    }

    private static void recordSuspiciousNotificationWarningInteractions(
            @SuspiciousNotificationWarningInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                value,
                SuspiciousNotificationWarningInteractions.MAX_VALUE);
    }

    /**
     * Logs the `interaction` for UMA if the `notificationId` from `origin` is suspicious. This UMA
     * is only accurate when the interaction happens in the same Chrome Activity that created the
     * notification.
     */
    private static boolean recordInteractionForUMAIfSuspicious(
            String origin,
            String notificationId,
            @SuspiciousNotificationWarningInteractions int interaction) {
        // Record only if triggered from a suspicious notification.
        if (isNotificationSuspicious(notificationId, origin)) {
            recordSuspiciousNotificationWarningInteractions(interaction);
            return true;
        }
        return false;
    }

    private static @Nullable String getStringFromBackupBundle(
            @Nullable Bundle notificationExtras, String extraType) {
        if (notificationExtras == null || !notificationExtras.containsKey(extraType)) {
            return "";
        }

        return (String) notificationExtras.getString(extraType);
    }

    private static boolean getBooleanFromBackupBundle(
            @Nullable Bundle notificationExtras, String extraType, boolean defaultValue) {
        if (notificationExtras == null || !notificationExtras.containsKey(extraType)) {
            return defaultValue;
        }

        return (boolean) notificationExtras.getBoolean(extraType);
    }

    private static void appendUnsubscribeButton(
            Notification.Builder notificationBuilder,
            String id,
            String origin,
            @Nullable String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            @Nullable String webApkPackage,
            @Nullable String channelId) {
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
            @Nullable String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            @Nullable String webApkPackage,
            @Nullable String channelId) {
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

    // TODO(crbug.com/421199353): Consolidate logic used for creating `PendingIntentProvider` values
    // from the `NotificationContentDetectionManager` and `NotificationPlatformBridge`.
    private static PendingIntentProvider makePendingIntentForButton(
            String id,
            String origin,
            @Nullable String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            @Nullable String webApkPackage,
            @Nullable String channelId,
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
        Intent intent = new Intent(action, intentData);
        intent.setClass(
                context,
                NotificationServiceImpl.Receiver.class);

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
        return PendingIntentProvider.getBroadcast(
                context,
                NotificationConstants.PENDING_INTENT_REQUEST_CODE,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT,
                /* mutable= */ false);
    }

    // Helper method for displaying a notification that was suppressed, due to a suspicious verdict.
    // This includes storing the notification contents in a `Bundle` on the notification in case of
    // "Always allow", logic for possible future unsubscribe reporting as spam, and adding buttons.
    private static void displayOriginalNotification(
            Notification originalNotification,
            String notificationId,
            String notificationOrigin,
            @Nullable String scopeUrl,
            @Nullable String profileId,
            boolean incognito,
            @Nullable String webApkPackage,
            @Nullable String channelId) {
        Context context = ContextUtils.getApplicationContext();
        Notification.Builder builder =
                Notification.Builder.recoverBuilder(context, originalNotification);

        // Store original notification contents as an extra in order to restore the
        // original notification without "Always allow" button and also any other
        // notifications that has "Always allow" button.
        Bundle extras = new Bundle();
        extras.putParcelable(
                NotificationConstants.EXTRA_NOTIFICATION_BACKUP_FOR_SUSPICIOUS_VERDICT,
                originalNotification.clone());
        // Store extra signaling that the user has seen the original notification.
        // This will help determine whether to allow the reporting feature from the
        // unsubscribe confirmation notification or not.
        extras.putBoolean(
                NotificationConstants.EXTRA_ALLOW_REPORTING_AS_SPAM_IS_NOTIFICATION_WARNED, true);
        builder.addExtras(extras);

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
        NotificationPlatformBridge.displayNotificationSilently(builder, notificationId);
    }
}
