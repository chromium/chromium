// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.serial;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/** Creates and destroys the WebSerial notification when a website is connected to a serial port. */
@NullMarked
public class SerialNotificationManager {
    private static final String NOTIFICATION_NAMESPACE = "SerialNotificationManager";

    public static final String ACTION_SERIAL_UPDATE =
            "org.chromium.chrome.browser.app.serial.SERIAL_UPDATE";

    public static final String NOTIFICATION_ID_EXTRA = "NotificationId";
    public static final String NOTIFICATION_IS_CONNECTED = "NotificationIsConnected";
    public static final String NOTIFICATION_IS_INCOGNITO = "NotificationIsIncognito";
    public static final String NOTIFICATION_URL_EXTRA = "NotificationUrl";

    private final SerialNotificationManagerDelegate mDelegate;
    private final BaseNotificationManagerProxy mNotificationManager;
    private final SharedPreferencesManager mSharedPreferences;
    private final List<Integer> mNotificationIds = new ArrayList<>();

    public SerialNotificationManager(
            BaseNotificationManagerProxy notificationManager,
            SerialNotificationManagerDelegate delegate) {
        mDelegate = delegate;
        mNotificationManager = notificationManager;
        mSharedPreferences = ChromeSharedPreferences.getInstance();
    }

    /**
     * @param notificationId Unique id of the notification.
     * @return Whether the notification has already been created for the provided notification id.
     */
    private boolean doesNotificationExist(int notificationId) {
        return mNotificationIds.contains(notificationId);
    }

    public void onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (intent == null || intent.getExtras() == null) {
            cancelPreviousSerialNotifications();
            mDelegate.stopSelf();
        } else if (ACTION_SERIAL_UPDATE.equals(intent.getAction())) {
            int notificationId = intent.getIntExtra(NOTIFICATION_ID_EXTRA, Tab.INVALID_TAB_ID);
            boolean isConnected = intent.getBooleanExtra(NOTIFICATION_IS_CONNECTED, false);
            String url = assertNonNull(intent.getStringExtra(NOTIFICATION_URL_EXTRA));
            boolean isIncognito = intent.getBooleanExtra(NOTIFICATION_IS_INCOGNITO, false);
            updateNotification(notificationId, isConnected, url, isIncognito, startId);
        }
    }

    /**
     * Cancel all previously existing notifications. Essential while doing a clean start (may be
     * after a browser crash which caused old notifications to exist).
     */
    public void cancelPreviousSerialNotifications() {
        Set<String> notificationIds =
                mSharedPreferences.readStringSet(
                        ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS, null);
        if (notificationIds == null) return;
        Iterator<String> iterator = notificationIds.iterator();
        while (iterator.hasNext()) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, Integer.parseInt(iterator.next()));
        }
        mSharedPreferences.removeKey(ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS);
    }

    /**
     * Destroy the existing notification if it exists and creates one if isConnected is true for the
     * provided notificationId.
     *
     * @param notificationId Unique id of the notification.
     * @param isConnected Whether the website is connected to a serial port.
     * @param url Url of the website interacting with serial port.
     * @param isIncognito Whether the notification comes from incognito mode.
     * @param startId Id for the service start request
     */
    private void updateNotification(
            int notificationId, boolean isConnected, String url, boolean isIncognito, int startId) {
        destroyNotification(notificationId);
        if (isConnected) createNotification(notificationId, url, isIncognito);
        if (mNotificationIds.size() == 0) mDelegate.stopSelf(startId);
    }

    /**
     * Destroys the notification for the id notificationId.
     *
     * @param notificationId Unique id of the notification.
     */
    private void destroyNotification(int notificationId) {
        if (!doesNotificationExist(notificationId)) return;
        mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
        mNotificationIds.remove(mNotificationIds.indexOf(notificationId));
        updateSharedPreferencesEntry(notificationId, true);
    }

    /**
     * Create a serial port notification for the provided notificationId.
     *
     * @param notificationId Unique id of the notification.
     * @param url Url of the website interacting with serial ports.
     * @param isIncognito Whether the notification comes from incognito mode.
     */
    private void createNotification(int notificationId, String url, boolean isIncognito) {
        Context appContext = ContextUtils.getApplicationContext();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.SERIAL,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.SERIAL,
                                NOTIFICATION_NAMESPACE,
                                notificationId));

        Intent tabIntent = mDelegate.createTrustedBringTabToFrontIntent(notificationId);
        PendingIntentProvider contentIntent =
                tabIntent == null
                        ? null
                        : PendingIntentProvider.getActivity(
                                appContext, notificationId, tabIntent, 0);

        builder.setAutoCancel(false)
                .setOngoing(true)
                .setLocalOnly(true)
                .setContentIntent(contentIntent)
                .setSmallIcon(R.drawable.gm_filled_developer_board_24)
                .setContentTitle(
                        appContext.getString(R.string.connected_to_serial_port_notification_title));

        String contentText = null;
        if (isIncognito) {
            contentText = appContext.getString(R.string.serial_notification_content_text_incognito);
            builder.setSubText(appContext.getString(R.string.notification_incognito_tab));
        } else {
            String urlForDisplay =
                    UrlFormatter.formatUrlForSecurityDisplay(
                            new GURL(url), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            if (contentIntent == null) {
                contentText = urlForDisplay;
            } else {
                contentText =
                        appContext.getString(
                                R.string.serial_notification_content_text, urlForDisplay);
            }
        }

        builder.setContentText(contentText);
        NotificationWrapper notification = builder.buildWithBigTextStyle(contentText);

        mNotificationManager.notify(notification);
        mNotificationIds.add(notificationId);
        updateSharedPreferencesEntry(notificationId, false);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.SERIAL,
                        notification.getNotification());
    }

    /**
     * Update shared preferences entry with ids of the visible notifications.
     *
     * @param notificationId Id of the notification.
     * @param remove Boolean describing if the notification was added or removed.
     */
    private void updateSharedPreferencesEntry(int notificationId, boolean remove) {
        Set<String> notificationIds =
                new HashSet<>(
                        mSharedPreferences.readStringSet(
                                ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS, new HashSet<>()));
        if (remove
                && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(notificationId))) {
            notificationIds.remove(String.valueOf(notificationId));
        } else if (!remove) {
            notificationIds.add(String.valueOf(notificationId));
        }
        mSharedPreferences.writeStringSet(
                ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS, notificationIds);
    }

    private static boolean shouldStartService(boolean isConnected, int notificationTabId) {
        if (isConnected) return true;
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return false;
        return notificationIds.contains(String.valueOf(notificationTabId));
    }

    /**
     * Send an intent to the serial notification service to either create or destroy the
     * notification identified by notificationTabId.
     *
     * @param context The activity context.
     * @param service The serial notification service class.
     * @param notificationTabId The tab id.
     * @param webContents The webContents for the tab. Used to query the serial state.
     * @param url Url of the website interacting with serial ports.
     * @param isIncognito Whether tab is in incognito mode.
     */
    public static void updateSerialNotificationForTab(
            Context context,
            Class service,
            int notificationTabId,
            @Nullable WebContents webContents,
            GURL url,
            boolean isIncognito) {
        boolean isConnected = SerialBridge.isWebContentsConnectedToSerialPort(webContents);
        if (!shouldStartService(isConnected, notificationTabId)) return;
        Intent intent = new Intent(context, service);
        intent.setAction(ACTION_SERIAL_UPDATE);
        intent.putExtra(NOTIFICATION_ID_EXTRA, notificationTabId);
        intent.putExtra(NOTIFICATION_URL_EXTRA, url.getSpec());
        intent.putExtra(NOTIFICATION_IS_CONNECTED, isConnected);
        intent.putExtra(NOTIFICATION_IS_INCOGNITO, isIncognito);
        context.startService(intent);
    }

    /**
     * Clear any previous serial notifications.
     *
     * @param service The serial notification service class.
     */
    public static void clearSerialNotifications(Class service) {
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(ChromePreferenceKeys.SERIAL_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return;
        Context context = ContextUtils.getApplicationContext();
        context.startService(new Intent(context, service));
    }
}
