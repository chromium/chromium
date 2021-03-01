// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.notification;

import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationInterval;
import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTextBody;
import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTitle;
import static org.chromium.chrome.browser.omaha.UpdateConfigs.isUpdateNotificationEnabled;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_AVAILABLE;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.omaha.OmahaBase;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Class supports to build and to send update notification every three weeks if new Chrome version
 * is available. It listens to {@link UpdateStatusProvider}, and handle the intent to start update
 * flow.
 */
public class UpdateNotificationControllerImpl implements UpdateNotificationController, Destroyable {
    private static final String TAG = "UpdateNotif";
    private static final String INLINE_UPDATE_NOTIFICATION_RECEIVED_EXTRA =
            "org.chromium.chrome.browser.omaha.inline_update_notification_received_extra";
    private static final String UPDATE_NOTIFICATION_STATE_EXTRA =
            "org.chromium.chrome.browser.omaha.update_notification_state_extra";
    private static final String UPDATE_NOTIFICATION_TAG =
            "org.chromium.chrome.browser.omaha.update_notification_tag";
    public static final String PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY =
            "pref_last_timestamp_update_notification_pushed_key";
    private final Callback<UpdateStatusProvider.UpdateStatus> mObserver = status -> {
        mUpdateStatus = status;
        processUpdateStatus();
    };

    private Activity mActivity;
    private ActivityLifecycleDispatcher mActivityLifecycle;
    private boolean mShouldStartInlineUpdate;
    private @Nullable UpdateStatus mUpdateStatus;

    /**
     * @param activity Activity the notification will be shown in.
     * @param lifecycleDispatcher Lifecycle of an Activity the notification will be shown in.
     */
    public UpdateNotificationControllerImpl(
            Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mActivityLifecycle = lifecycleDispatcher;
        UpdateStatusProvider.getInstance().addObserver(mObserver);
        mActivityLifecycle.register(this);
    }

    // UpdateNotificationController implementation.
    @Override
    public void onNewIntent(Intent intent) {
        mShouldStartInlineUpdate =
                intent.getBooleanExtra(INLINE_UPDATE_NOTIFICATION_RECEIVED_EXTRA, false);
        processUpdateStatus();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        UpdateStatusProvider.getInstance().removeObserver(mObserver);
        mActivityLifecycle.unregister(this);
        mActivityLifecycle = null;
        mActivity = null;
    }

    private void processUpdateStatus() {
        if (mUpdateStatus == null) return;

        switch (mUpdateStatus.updateState) {
            case UPDATE_AVAILABLE:
                scheduleUpdateNotification();
                break;
            case INLINE_UPDATE_AVAILABLE:
                if (mShouldStartInlineUpdate) {
                    UpdateStatusProvider.getInstance().startInlineUpdate(
                            UpdateStatusProvider.UpdateInteractionSource.FROM_NOTIFICATION,
                            mActivity);
                    mShouldStartInlineUpdate = false;
                } else {
                    scheduleUpdateNotification();
                }
                break;
            default:
                break;
        }
    }

    private void scheduleUpdateNotification() {
        if (!shouldPushNotification()) return;

        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory
                        .createNotificationWrapperBuilder(true,
                                ChromeChannelDefinitions.ChannelId.UPDATES, null,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.UPDATES,
                                        UPDATE_NOTIFICATION_TAG /* notificationTag */,
                                        NotificationConstants.NOTIFICATION_ID_UPDATE))
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setAutoCancel(true)
                        .setContentTitle(getUpdateNotificationTitle())
                        .setContentText(getUpdateNotificationTextBody());

        builder.setContentIntent(createContentIntent(mUpdateStatus));
        NotificationWrapper notification = builder.buildNotificationWrapper();
        NotificationManagerProxy notificationManager = new NotificationManagerProxyImpl(mActivity);
        notificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.UPDATES,
                notification.getNotification());
        updateLastPushedTimeStamp();
    }

    private PendingIntentProvider createContentIntent(UpdateStatus status) {
        Context context = ContextUtils.getApplicationContext();
        Intent clickIntent = new Intent(context, UpdateNotificationReceiver.class)
                                     .putExtra(UPDATE_NOTIFICATION_STATE_EXTRA, status.updateState);
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(
                context, 0, clickIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        return contentIntent;
    }

    private static boolean shouldPushNotification() {
        if (!isUpdateNotificationEnabled()) return false;
        long currentTime = System.currentTimeMillis();
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        long lastPushedTimeStamp = preferences.getLong(PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY, 0);
        return currentTime - lastPushedTimeStamp >= getUpdateNotificationInterval();
    }

    private static void updateLastPushedTimeStamp() {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putLong(PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY, System.currentTimeMillis());
        editor.apply();
    }

    /**
     * A receiver that try to build the intent to launch Chrome activity.
     */
    public static final class UpdateNotificationReceiver extends BroadcastReceiver {
        // BroadcastReceiver implementation.
        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserParts parts = new EmptyBrowserParts() {
                @Override
                public void finishNativeInitialization() {
                    try {
                        int state = intent.getIntExtra(UPDATE_NOTIFICATION_STATE_EXTRA,
                                UpdateStatusProvider.UpdateState.NONE);
                        UpdateUtils.onUpdateAvailable(context, state);
                    } catch (IllegalArgumentException e) {
                        // If it takes too long to load native library, we may fail to start
                        // activity.
                        Log.e(TAG, "Failed to start activity in background.", e);
                    }
                }
            };

            // Try to load native.
            ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        }
    }
}
