// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.support.v4.app.ServiceCompat;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.notifications.ForegroundServiceUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Keep-alive foreground service for downloads.
 */
public class DownloadForegroundService extends Service {
    private static final String TAG = "DownloadFg";
    // Deprecated, remove this after M75.
    private static final String KEY_PERSISTED_NOTIFICATION_ID = "PersistedNotificationId";
    private final IBinder mBinder = new LocalBinder();

    private NotificationManager mNotificationManager;

    @IntDef({StopForegroundNotification.KILL, StopForegroundNotification.DETACH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StopForegroundNotification {
        int KILL = 0; // Kill notification regardless of ability to detach.
        int DETACH = 1; // Try to detach, otherwise kill and relaunch.
    }

    @Override
    public void onCreate() {
        super.onCreate();

        mNotificationManager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        clearPersistedNotificationId();
    }

    /**
     * Start the foreground service with this given context.
     * @param context The context used to start service.
     */
    public static void startDownloadForegroundService(Context context) {
        // TODO(crbug.com/770389): Grab a WakeLock here until the service has started.
        ForegroundServiceUtils.getInstance().startForegroundService(
                new Intent(context, DownloadForegroundService.class));
    }

    /**
     * Start the foreground service or update it to be pinned to a different notification.
     *
     * @param newNotificationId   The ID of the new notification to pin the service to.
     * @param newNotification     The new notification to be pinned to the service.
     * @param oldNotificationId   The ID of the original notification that was pinned to the
     *                            service, can be INVALID_NOTIFICATION_ID if the service is just
     *                            starting.
     * @param oldNotification     The original notification the service was pinned to, in case an
     *                            adjustment needs to be made (in the case it could not be
     *                            detached).
     * @param killOldNotification Whether or not to detach or kill the old notification.
     */
    public void startOrUpdateForegroundService(int newNotificationId, Notification newNotification,
            int oldNotificationId, Notification oldNotification, boolean killOldNotification) {
        Log.w(TAG,
                "startOrUpdateForegroundService new: " + newNotificationId
                        + ", old: " + oldNotificationId + ", kill old: " + killOldNotification);
        // Handle notifications and start foreground.
        if (oldNotificationId == INVALID_NOTIFICATION_ID && oldNotification == null) {
            // If there is no old notification or old notification id, just start foreground.
            startForegroundInternal(newNotificationId, newNotification);
        } else {
            if (getCurrentSdk() >= 24) {
                // If possible, detach notification so it doesn't get cancelled by accident.
                stopForegroundInternal(killOldNotification ? ServiceCompat.STOP_FOREGROUND_REMOVE
                                                           : ServiceCompat.STOP_FOREGROUND_DETACH);
                startForegroundInternal(newNotificationId, newNotification);
            } else {
                // Otherwise start the foreground and relaunch the originally pinned notification.
                startForegroundInternal(newNotificationId, newNotification);
                if (!killOldNotification) {
                    relaunchOldNotification(oldNotificationId, oldNotification);
                }
            }
        }

        // Record when starting foreground and when updating pinned notification.
        if (oldNotificationId == INVALID_NOTIFICATION_ID) {
            DownloadNotificationUmaHelper.recordForegroundServiceLifecycleHistogram(
                    DownloadNotificationUmaHelper.ForegroundLifecycle.START);
        } else {
            if (oldNotificationId != newNotificationId) {
                DownloadNotificationUmaHelper.recordForegroundServiceLifecycleHistogram(
                        DownloadNotificationUmaHelper.ForegroundLifecycle.UPDATE);
            }
        }
    }

    /**
     * Stop the foreground service that is running.
     *
     * @param stopForegroundNotification    How to handle the notification upon the foreground
     *                                      stopping (options are: kill, detach or adjust, or detach
     *                                      or persist, see {@link StopForegroundNotification}.
     * @param pinnedNotificationId          Id of the notification that is pinned to the foreground
     *                                      and would need adjustment.
     * @param pinnedNotification            The actual notification that is pinned to the foreground
     *                                      and would need adjustment.
     */
    public void stopDownloadForegroundService(
            @StopForegroundNotification int stopForegroundNotification, int pinnedNotificationId,
            Notification pinnedNotification) {
        Log.w(TAG,
                "stopDownloadForegroundService status: " + stopForegroundNotification
                        + ", id: " + pinnedNotificationId);
        // Record when stopping foreground.
        DownloadNotificationUmaHelper.recordForegroundServiceLifecycleHistogram(
                DownloadNotificationUmaHelper.ForegroundLifecycle.STOP);
        DownloadNotificationUmaHelper.recordServiceStoppedHistogram(
                DownloadNotificationUmaHelper.ServiceStopped.STOPPED, true /* withForeground */);

        // Handle notifications and stop foreground.
        if (stopForegroundNotification == StopForegroundNotification.KILL) {
            // Regardless of the SDK level, stop foreground and kill if so indicated.
            stopForegroundInternal(ServiceCompat.STOP_FOREGROUND_REMOVE);
        } else {
            if (getCurrentSdk() >= 24) {
                // Android N+ has the option to detach notifications from the service, so detach or
                // kill the notification as needed when stopping service.
                stopForegroundInternal(ServiceCompat.STOP_FOREGROUND_DETACH);
            } else if (getCurrentSdk() >= 23) {
                // Android M+ can't detach the notification but doesn't have other caveats. Kill the
                // notification and relaunch if detach was desired.
                stopForegroundInternal(ServiceCompat.STOP_FOREGROUND_REMOVE);
                relaunchOldNotification(pinnedNotificationId, pinnedNotification);
            } else {
                // In phones that are Lollipop and older (API < 23), the service gets killed with
                // the task, which might result in the notification being unable to be relaunched
                // where it needs to be. kill and relaunch the old notification before stopping the
                // service.
                relaunchOldNotification(
                        getNewNotificationIdFor(pinnedNotificationId), pinnedNotification);
                stopForegroundInternal(ServiceCompat.STOP_FOREGROUND_REMOVE);
            }
        }
        stopSelf();
    }

    @VisibleForTesting
    void relaunchOldNotification(int notificationId, Notification notification) {
        if (notificationId != INVALID_NOTIFICATION_ID && notification != null) {
            mNotificationManager.notify(notificationId, notification);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // In the case the service was restarted when the intent is null.
        if (intent == null) {
            DownloadNotificationUmaHelper.recordServiceStoppedHistogram(
                    DownloadNotificationUmaHelper.ServiceStopped.START_STICKY, true);

            // Allow observers to restart service on their own, if needed.
            stopSelf();
        }

        // This should restart service after Chrome gets killed (except for Android 4.4.2).
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        DownloadNotificationUmaHelper.recordServiceStoppedHistogram(
                DownloadNotificationUmaHelper.ServiceStopped.DESTROYED, true /* withForeground */);
        DownloadForegroundServiceObservers.alertObserversServiceDestroyed();
        super.onDestroy();
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        DownloadNotificationUmaHelper.recordServiceStoppedHistogram(
                DownloadNotificationUmaHelper.ServiceStopped.TASK_REMOVED, true /*withForeground*/);
        DownloadForegroundServiceObservers.alertObserversTaskRemoved();
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onLowMemory() {
        DownloadNotificationUmaHelper.recordServiceStoppedHistogram(
                DownloadNotificationUmaHelper.ServiceStopped.LOW_MEMORY, true /* withForeground */);
        super.onLowMemory();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    /**
     * Class for clients to access.
     */
    class LocalBinder extends Binder {
        DownloadForegroundService getService() {
            return DownloadForegroundService.this;
        }
    }


    /**
     * Clear stored value for the id of the notification pinned to the service.
     */
    @VisibleForTesting
    static void clearPersistedNotificationId() {
        ContextUtils.getAppSharedPreferences().edit().remove(KEY_PERSISTED_NOTIFICATION_ID).apply();
    }

    /** Methods for testing. */

    @VisibleForTesting
    int getNewNotificationIdFor(int oldNotificationId) {
        return DownloadNotificationService.getNewNotificationIdFor(oldNotificationId);
    }

    @VisibleForTesting
    void startForegroundInternal(int notificationId, Notification notification) {
        Log.w(TAG, "startForegroundInternal id: " + notificationId);
        ForegroundServiceUtils.getInstance().startForeground(
                this, notificationId, notification, 0 /* foregroundServiceType */);
    }

    @VisibleForTesting
    void stopForegroundInternal(int flags) {
        Log.w(TAG, "stopForegroundInternal flags: " + flags);
        ForegroundServiceUtils.getInstance().stopForeground(this, flags);
    }

    @VisibleForTesting
    int getCurrentSdk() {
        return Build.VERSION.SDK_INT;
    }
}
