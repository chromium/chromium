// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bluetooth;

import android.content.Context;
import android.content.Intent;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
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
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

/**
 * Creates and destroys the Web Bluetooth notification when a website is either connected
 * to a Bluetooth device or scanning for nearby Bluetooth devices.
 */
public class BluetoothNotificationManager {
    private static final String NOTIFICATION_NAMESPACE = "BluetoothNotificationManager";

    public static final String ACTION_BLUETOOTH_UPDATE =
            "org.chromium.chrome.browser.app.bluetooth.BLUETOOTH_UPDATE";

    public static final String NOTIFICATION_ID_EXTRA = "NotificationId";
    public static final String NOTIFICATION_IS_INCOGNITO = "NotificationIsIncognito";
    public static final String NOTIFICATION_BLUETOOTH_TYPE_EXTRA = "NotificationBluetoothType";
    public static final String NOTIFICATION_URL_EXTRA = "NotificationUrl";

    @IntDef({BluetoothType.NO_BLUETOOTH, BluetoothType.IS_CONNECTED, BluetoothType.IS_SCANNING})
    public @interface BluetoothType {
        int NO_BLUETOOTH = 0;
        int IS_CONNECTED = 1;
        int IS_SCANNING = 2;
    }

    private BluetoothNotificationManagerDelegate mDelegate;
    private BaseNotificationManagerProxy mNotificationManager;
    private SharedPreferencesManager mSharedPreferences;
    private final SparseIntArray mNotifications = new SparseIntArray();

    public BluetoothNotificationManager(
            BaseNotificationManagerProxy notificationManager,
            BluetoothNotificationManagerDelegate delegate) {
        mDelegate = delegate;
        mNotificationManager = notificationManager;
        mSharedPreferences = ChromeSharedPreferences.getInstance();
    }

    /**
     * @param notificationId Unique id of the notification.
     * @param bluetoothType Bluetooth type of the notification.
     * @return Whether the notification has already been created for provided notification id and
     *         bluetoothType.
     */
    private boolean doesNotificationNeedUpdate(
            int notificationId, @BluetoothType int bluetoothType) {
        return mNotifications.get(notificationId) != bluetoothType;
    }

    /**
     * @param notificationId Unique id of the notification.
     * @return Whether the notification has already been created for the provided notification id.
     */
    private boolean doesNotificationExist(int notificationId) {
        return mNotifications.indexOfKey(notificationId) >= 0;
    }

    public void onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || intent.getExtras() == null) {
            cancelPreviousBluetoothNotifications();
            mDelegate.stopSelf();
        } else if (ACTION_BLUETOOTH_UPDATE.equals(intent.getAction())) {
            int notificationId = intent.getIntExtra(NOTIFICATION_ID_EXTRA, Tab.INVALID_TAB_ID);
            int bluetoothType =
                    intent.getIntExtra(
                            NOTIFICATION_BLUETOOTH_TYPE_EXTRA, BluetoothType.NO_BLUETOOTH);
            String url = intent.getStringExtra(NOTIFICATION_URL_EXTRA);
            boolean isIncognito = intent.getBooleanExtra(NOTIFICATION_IS_INCOGNITO, false);
            updateNotification(notificationId, bluetoothType, url, isIncognito, startId);
        }
    }

    /**
     * Cancel all previously existing notifications. Essential while doing a clean start (may be
     * after a browser crash which caused old notifications to exist).
     */
    public void cancelPreviousBluetoothNotifications() {
        Set<String> notificationIds =
                mSharedPreferences.readStringSet(
                        ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS, null);
        if (notificationIds == null) return;
        Iterator<String> iterator = notificationIds.iterator();
        while (iterator.hasNext()) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, Integer.parseInt(iterator.next()));
        }
        mSharedPreferences.removeKey(ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS);
    }

    /**
     * Updates the existing notification or creates one if none exist for the provided
     * notificationId and bluetoothType.
     * @param notificationId Unique id of the notification.
     * @param bluetoothType Bluetooth type of the notification.
     * @param url Url of the website interacting with Bluetooth devices.
     * @param isIncognito Whether the notification comes from incognito mode.
     * @param startId Id for the service start request
     */
    private void updateNotification(
            int notificationId,
            @BluetoothType int bluetoothType,
            String url,
            boolean isIncognito,
            int startId) {
        if (doesNotificationExist(notificationId)
                && !doesNotificationNeedUpdate(notificationId, bluetoothType)) {
            return;
        }
        destroyNotification(notificationId);
        if (bluetoothType != BluetoothType.NO_BLUETOOTH) {
            createNotification(notificationId, bluetoothType, url, isIncognito);
        }
        if (mNotifications.size() == 0) mDelegate.stopSelf(startId);
    }

    /**
     * Destroys the notification for the id notificationId.
     * @param notificationId Unique id of the notification.
     */
    private void destroyNotification(int notificationId) {
        if (doesNotificationExist(notificationId)) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
            mNotifications.delete(notificationId);
            updateSharedPreferencesEntry(notificationId, true);
        }
    }

    /**
     * Create a Bluetooth notification for the provided
     * notificationId and bluetoothType.
     * @param notificationId Unique id of the notification.
     * @param bluetoothType Bluetooth type of the notification.
     * @param url Url of the website interacting with Bluetooth devices.
     * @param isIncognito Whether the notification comes from incognito mode.
     */
    private void createNotification(
            int notificationId, @BluetoothType int bluetoothType, String url, boolean isIncognito) {
        Context appContext = ContextUtils.getApplicationContext();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.BLUETOOTH,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.BLUETOOTH,
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
                .setSmallIcon(getNotificationIconId(bluetoothType))
                .setContentTitle(getNotificationTitleText(bluetoothType));

        String contentText = null;
        if (isIncognito) {
            contentText =
                    appContext.getString(R.string.bluetooth_notification_content_text_incognito);
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
                                R.string.bluetooth_notification_content_text, urlForDisplay);
            }
        }

        builder.setContentText(contentText);
        NotificationWrapper notification = builder.buildWithBigTextStyle(contentText);

        mNotificationManager.notify(notification);
        mNotifications.put(notificationId, bluetoothType);
        updateSharedPreferencesEntry(notificationId, false);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.BLUETOOTH,
                        notification.getNotification());
    }

    /**
     * @param bluetoothType Bluetooth type of the notification.
     * @return user-facing text for the provided bluetoothType.
     */
    private String getNotificationTitleText(@BluetoothType int bluetoothType) {
        int notificationContentTextId = 0;
        if (bluetoothType == BluetoothType.IS_CONNECTED) {
            notificationContentTextId = R.string.connected_to_bluetooth_device_notification_title;
        } else if (bluetoothType == BluetoothType.IS_SCANNING) {
            notificationContentTextId = R.string.scanning_for_bluetooth_devices_notification_title;
        }
        assert notificationContentTextId != 0;

        return ContextUtils.getApplicationContext().getString(notificationContentTextId);
    }

    /**
     * @param bluetoothType Bluetooth type of the notification.
     * @return An icon id of the provided bluetoothType.
     */
    private int getNotificationIconId(@BluetoothType int bluetoothType) {
        int notificationIconId = 0;
        if (bluetoothType == BluetoothType.IS_CONNECTED) {
            notificationIconId = R.drawable.ic_bluetooth_connected;
        } else if (bluetoothType == BluetoothType.IS_SCANNING) {
            notificationIconId = R.drawable.gm_filled_bluetooth_searching_24;
        }
        return notificationIconId;
    }

    /**
     * Update shared preferences entry with ids of the visible notifications.
     * @param notificationId Id of the notification.
     * @param remove Boolean describing if the notification was added or removed.
     */
    private void updateSharedPreferencesEntry(int notificationId, boolean remove) {
        Set<String> notificationIds =
                new HashSet<>(
                        mSharedPreferences.readStringSet(
                                ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS, new HashSet<>()));
        if (remove
                && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(notificationId))) {
            notificationIds.remove(String.valueOf(notificationId));
        } else if (!remove) {
            notificationIds.add(String.valueOf(notificationId));
        }
        mSharedPreferences.writeStringSet(
                ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS, notificationIds);
    }

    private static boolean shouldStartService(
            @BluetoothType int bluetoothType, int notificationTabId) {
        if (!ContentFeatureMap.isEnabled(
                ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
            return false;
        }
        if (bluetoothType != BluetoothType.NO_BLUETOOTH) return true;
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return false;
        return notificationIds.contains(String.valueOf(notificationTabId));
    }

    /**
     * Send an intent to the bluetooth notification service to either create or destroy the
     * notification identified by notificationTabId.
     *
     * @param context The activity context.
     * @param service The bluetooth notification service class.
     * @param notificationTabId The tab id.
     * @param webContents The webContents for the tab. Used to query the Bluetooth state.
     * @param url Url of the website interacting with Bluetooth devices.
     * @param isIncognito Whether tab is in incognito mode.
     */
    public static void updateBluetoothNotificationForTab(
            Context context,
            Class service,
            int notificationTabId,
            @Nullable WebContents webContents,
            GURL url,
            boolean isIncognito) {
        @BluetoothType int bluetoothType = getBluetoothType(webContents);
        if (!shouldStartService(bluetoothType, notificationTabId)) return;
        Intent intent = new Intent(context, service);
        intent.setAction(ACTION_BLUETOOTH_UPDATE);
        intent.putExtra(NOTIFICATION_ID_EXTRA, notificationTabId);
        intent.putExtra(NOTIFICATION_URL_EXTRA, url.getSpec());
        intent.putExtra(NOTIFICATION_BLUETOOTH_TYPE_EXTRA, bluetoothType);
        intent.putExtra(NOTIFICATION_IS_INCOGNITO, isIncognito);
        context.startService(intent);
    }

    /**
     * Clear any previous Bluetooth notifications.
     * @param service The bluetooth notification service class.
     */
    public static void clearBluetoothNotifications(Class service) {
        if (!ContentFeatureMap.isEnabled(
                ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
            return;
        }
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.BLUETOOTH_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return;

        Context context = ContextUtils.getApplicationContext();
        context.startService(new Intent(context, service));
    }

    /**
     * @param webContents the webContents for the tab. Used to query the Bluetooth state.
     * @return A constant identifying the kind of Bluetooth interaction.
     */
    private static @BluetoothType int getBluetoothType(@Nullable WebContents webContents) {
        if (webContents == null) {
            return BluetoothType.NO_BLUETOOTH;
        }

        if (BluetoothBridge.isWebContentsScanningForBluetoothDevices(webContents)) {
            return BluetoothType.IS_SCANNING;
        }

        if (BluetoothBridge.isWebContentsConnectedToBluetoothDevice(webContents)) {
            return BluetoothType.IS_CONNECTED;
        }

        return BluetoothType.NO_BLUETOOTH;
    }
}
