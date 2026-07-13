// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import org.chromium.base.Log;
import org.chromium.base.SplitCompatService;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Service implementation that keeps Chrome elevated at Foreground Service priority for a fixed
 * duration during window closure to allow graceful tab closure (unload handlers, WebRTC
 * disconnection, etc.).
 */
@NullMarked
public class GracefulShutdownServiceImpl extends SplitCompatService.Impl {
    private static final String TAG = "GracefulShutdown";
    private static final String NOTIFICATION_NAMESPACE = "GracefulShutdownService";
    // 2000 ms was chosen to match the 2-second fallback timer in TabWebContentsDestroyer
    // (see chrome/browser/android/tab_android.cc), which was picked to give sufficient time for
    // unload handlers to run during window closure.
    private static final int DEFAULT_SHUTDOWN_TIMEOUT_MS = 2000;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private @Nullable Runnable mStopRunnable;

    @Override
    public void onCreate() {
        Log.i(TAG, "GracefulShutdownServiceImpl onCreate");
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        Log.i(TAG, "GracefulShutdownServiceImpl onStartCommand");
        try {
            new ChannelsInitializer(
                            new NotificationManagerProxyImpl(),
                            ChromeChannelDefinitions.getInstance(),
                            getService().getResources())
                    .safeInitialize(ChromeChannelDefinitions.ChannelId.BROWSER);
            NotificationWrapper notification =
                    NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                    ChromeChannelDefinitions.ChannelId.BROWSER,
                                    new NotificationMetadata(
                                            NotificationUmaTracker.SystemNotificationType.UNKNOWN,
                                            NOTIFICATION_NAMESPACE,
                                            NotificationConstants
                                                    .NOTIFICATION_ID_GRACEFUL_SHUTDOWN))
                            .setSmallIcon(R.drawable.ic_chrome)
                            .setLocalOnly(/* localOnly= */ true)
                            .setOngoing(/* ongoing= */ true)
                            .setSilent(/* silent= */ true)
                            .setContentTitle(
                                    getService().getString(R.string.graceful_shutdown_title))
                            .buildNotificationWrapper();

            int fgsType = 0;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                fgsType = ServiceInfo.FOREGROUND_SERVICE_TYPE_SHORT_SERVICE;
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                fgsType = ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC;
            }

            ForegroundServiceUtils.getInstance()
                    .startForeground(
                            getService(),
                            NotificationConstants.NOTIFICATION_ID_GRACEFUL_SHUTDOWN,
                            notification.getNotification(),
                            fgsType);
            GracefulShutdownService.recordStatus(GracefulShutdownService.Status.STARTED);
        } catch (Throwable e) {
            GracefulShutdownService.recordStatus(
                    GracefulShutdownService.Status.START_FOREGROUND_FAILED);
            Log.e(TAG, "Failed to startForeground", e);
        }

        if (mStopRunnable != null) {
            mHandler.removeCallbacks(mStopRunnable);
        }

        mStopRunnable =
                () -> {
                    Log.i(TAG, "GracefulShutdownServiceImpl stopping");
                    GracefulShutdownService.recordStatus(GracefulShutdownService.Status.TIMED_OUT);
                    try {
                        ForegroundServiceUtils.getInstance()
                                .stopForeground(getService(), Service.STOP_FOREGROUND_REMOVE);
                    } catch (Throwable e) {
                        Log.e(TAG, "Failed to stopForeground", e);
                    }
                    getService().stopSelf();
                };
        mHandler.postDelayed(mStopRunnable, DEFAULT_SHUTDOWN_TIMEOUT_MS);

        return Service.START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (mStopRunnable != null) {
            mHandler.removeCallbacks(mStopRunnable);
            mStopRunnable = null;
        }
        Log.i(TAG, "GracefulShutdownServiceImpl onDestroy");
    }

    void setServiceForTesting(SplitCompatService service) {
        setService(service);
    }
}
