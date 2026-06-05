// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Binder;
import android.os.IBinder;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;
import androidx.core.app.ServiceCompat;

import org.chromium.base.SplitCompatService;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorForegroundServiceUmaHelper.ForegroundLifecycle;
import org.chromium.chrome.browser.actor.ActorForegroundServiceUmaHelper.StopReason;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;

/** Implementation of ActorForegroundService. */
@NullMarked
public class ActorForegroundServiceImpl extends SplitCompatService.Impl {
    private final IBinder mBinder = new LocalBinder();
    private long mStartTime;
    private boolean mIsForeground;
    private boolean mStopReasonRecorded;

    /**
     * Start the foreground service with this given context.
     *
     * @param context The context used to start service.
     */
    public static void startActorForegroundService(Context context) {
        ForegroundServiceUtils.getInstance()
                .startForegroundService(new Intent(context, ActorForegroundService.class));
    }

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

        if (!mIsForeground) {
            ActorForegroundServiceUmaHelper.recordLifecycleHistogram(ForegroundLifecycle.STARTED);
            mIsForeground = true;
        } else {
            ActorForegroundServiceUmaHelper.recordLifecycleHistogram(ForegroundLifecycle.UPDATED);
        }

        startForegroundInternal(newNotificationId, newNotification);
    }

    /**
     * Stops the foreground state and the service itself.
     *
     * @param flags Flags for stopping foreground (e.g., ServiceCompat.STOP_FOREGROUND_REMOVE).
     */
    public void stopActorForegroundService(int flags) {
        if (mIsForeground) {
            ActorForegroundServiceUmaHelper.recordLifecycleHistogram(ForegroundLifecycle.STOPPED);
            mIsForeground = false;
        }
        recordStopReason(StopReason.STOPPED);
        stopForegroundInternal(flags);
        getService().stopSelf();
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (mStartTime == 0) {
            mStartTime = SystemClock.elapsedRealtime();
        }

        // Return START_NOT_STICKY so the system doesn't attempt to recreate the service if it is
        // killed.
        return Service.START_NOT_STICKY;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        recordStopReason(StopReason.TASK_REMOVED);
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onLowMemory() {
        recordStopReason(StopReason.LOW_MEMORY);
        super.onLowMemory();
    }

    @Override
    public void onDestroy() {
        if (mStartTime > 0) {
            ActorForegroundServiceUmaHelper.recordDurationHistogram(
                    SystemClock.elapsedRealtime() - mStartTime);
            recordStopReason(StopReason.DESTROYED);
        }

        // TODO(ritkagup) : Notify observers so they can perform cleanup or pause active tasks.
        super.onDestroy();
    }

    private void recordStopReason(@StopReason int stopReason) {
        if (mStopReasonRecorded || mStartTime == 0) return;
        ActorForegroundServiceUmaHelper.recordStopReasonHistogram(stopReason);
        mStopReasonRecorded = true;
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
    void setServiceForTesting(SplitCompatService service) {
        setService(service);
    }

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
