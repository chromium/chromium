// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.PasswordAccessLossNotificationProperties.ALL_KEYS;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossNotificationProperties.TEXT;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossNotificationProperties.TITLE;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Shows the password access loss notification with the properties belonging to the current {@link
 * PasswordAccessLossWarningType}.
 */
public class PwdAccessLossNotificationCoordinator {
    @VisibleForTesting protected static final String TAG = "access_loss_warning";

    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManagerProxy;

    @VisibleForTesting
    PwdAccessLossNotificationCoordinator(Context context, BaseNotificationManagerProxy manager) {
        mContext = context;
        mNotificationManagerProxy = manager;
    }

    public PwdAccessLossNotificationCoordinator(Context context) {
        this(context, BaseNotificationManagerProxyFactory.create(context));
    }

    private static NotificationWrapperBuilder createNotificationBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChannelId.BROWSER,
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.UPM_ACCESS_LOSS_WARNING,
                        TAG,
                        NotificationConstants.NOTIFICATION_ID_UPM_ACCESS_LOSS));
    }

    /**
     * Shows the password access loss notification for the given {@link
     * PasswordAccessLossWarningType}.
     *
     * @param warningType determines the looks of the notification.
     */
    public void showNotification(@PasswordAccessLossWarningType int warningType) {
        PropertyModel model = getModelForNotificationType(warningType);
        String title = model.get(TITLE);
        String contents = model.get(TEXT).toString();

        // TODO: crbug.com/354886479 - Add the notification actions.
        NotificationWrapperBuilder notificationWrapperBuilder =
                createNotificationBuilder()
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(false)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        .setContentTitle(title)
                        .setContentText(contents)
                        .setTicker(contents);

        NotificationWrapper notification =
                notificationWrapperBuilder.buildWithBigTextStyle(contents);

        mNotificationManagerProxy.notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.UPM_ACCESS_LOSS_WARNING,
                        notification.getNotification());
    }

    /**
     * Creates the model that has the text and functionality appropriate for the notification type.
     *
     * @param warningType determines the values in the models.
     * @return the model for the notification.
     */
    @Nullable
    PropertyModel getModelForNotificationType(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return buildAccessLossNotificationNoGms();
            case PasswordAccessLossWarningType.NO_UPM:
                // Fallthrough, same as ONLY_ACCOUNT_UPM.
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return buildAccessLossNotificationAboutGmsUpdate();
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return buildAccessLossNotificationAboutManualMigration();
            default:
                assert false : "Unhandled warning type.";
                return null;
        }
    }

    /**
     * GMS Core doesn't exist on the device so the user is asked to export their passwords.
     *
     * @return the model for the notification.
     */
    PropertyModel buildAccessLossNotificationNoGms() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(TITLE, mContext.getString(R.string.pwd_access_loss_warning_no_gms_core_title))
                .with(
                        TEXT,
                        mContext.getString(R.string.pwd_access_loss_notification_no_gms_core_text))
                .build();
    }

    /**
     * GMS Core on the device doesn't support UPM so the user is asked to update GMS Core.
     *
     * @return the model for the notification.
     */
    PropertyModel buildAccessLossNotificationAboutGmsUpdate() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        TITLE,
                        mContext.getString(R.string.pwd_access_loss_warning_update_gms_core_title))
                .with(
                        TEXT,
                        mContext.getString(
                                R.string.pwd_access_loss_notification_update_gms_core_text))
                .build();
    }

    /**
     * GMS Core version on the device is new enough for UPM, but the automatic migration failed, so
     * the user is asked to manually do the migration by performing export and import.
     *
     * @return the model for the notification.
     */
    PropertyModel buildAccessLossNotificationAboutManualMigration() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        TITLE,
                        mContext.getString(R.string.pwd_access_loss_warning_manual_migration_title))
                .with(
                        TEXT,
                        mContext.getString(R.string.pwd_access_loss_warning_manual_migration_text))
                .build();
    }
}
