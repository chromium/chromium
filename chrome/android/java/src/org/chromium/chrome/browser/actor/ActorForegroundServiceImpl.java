// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Binder;
import android.os.IBinder;

import androidx.annotation.VisibleForTesting;
import androidx.core.app.ServiceCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;

/** Implementation of ActorForegroundService. */
@NullMarked
public class ActorForegroundServiceImpl extends SplitCompatService.Impl {
    private final IBinder mBinder = new LocalBinder();

    /**
     * Starts or updates the foreground service with a notification.
     *
     * @param newNotificationId The ID for the new notification.
     * @param newNotification The notification to display.
     * @param oldNotificationId The ID of the previous notification, or -1 if none.
     * @param killOldNotification Whether to remove the old notification.
     */
    public void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification) {

        if (oldNotificationId != -1 && oldNotificationId != newNotificationId) {
            stopForegroundInternal(
                    killOldNotification
                            ? ServiceCompat.STOP_FOREGROUND_REMOVE
                            : ServiceCompat.STOP_FOREGROUND_DETACH);
        }

        startForegroundInternal(newNotificationId, newNotification);
    }

    /**
     * Stops the foreground state and the service itself.
     *
     * @param flags Flags for stopping foreground (e.g., ServiceCompat.STOP_FOREGROUND_REMOVE).
     */
    public void stopActorForegroundService(int flags) {
        stopForegroundInternal(flags);
        getService().stopSelf();
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        // Return START_NOT_STICKY so the system doesn't attempt to recreate the service if it is
        // killed.
        return Service.START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        // TODO(ritkagup) : Notify observers so they can perform cleanup or pause active tasks.
        super.onDestroy();
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return mBinder;
    }

    /** Local binder class to allow clients to access this service instance. */
    class LocalBinder extends Binder {
        ActorForegroundServiceImpl getService() {
            return ActorForegroundServiceImpl.this;
        }
    }

    /** Methods for testing. */
    @VisibleForTesting
    void startForegroundInternal(int notificationId, Notification notification) {
        ForegroundServiceUtils.getInstance()
                .startForeground(
                        getService(),
                        notificationId,
                        notification,
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
    }

    @VisibleForTesting
    void stopForegroundInternal(int flags) {
        ForegroundServiceUtils.getInstance().stopForeground(getService(), flags);
    }
}
