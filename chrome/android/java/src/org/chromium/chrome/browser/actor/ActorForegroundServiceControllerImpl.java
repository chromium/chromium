// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

/** Android implementation of {@link ActorForegroundServiceController}. */
@NullMarked
@ServiceImpl(ActorForegroundServiceController.class)
public class ActorForegroundServiceControllerImpl implements ActorForegroundServiceController {
    private static final String TAG = "ActorFgsController";

    private @Nullable ActorForegroundServiceImpl mBoundService;
    private @Nullable Runnable mOnConnectedRunnable;

    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    if (service instanceof ActorForegroundServiceImpl.LocalBinder binder) {
                        mBoundService = binder.getService();
                    } else {
                        Log.w(TAG, "Unexpected binder type.");
                    }

                    if (mOnConnectedRunnable != null) {
                        mOnConnectedRunnable.run();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName componentName) {
                    if (BuildConfig.ENABLE_ASSERTS) {
                        Log.i(TAG, "Service disconnected: " + componentName);
                    }
                    mBoundService = null;
                }
            };

    @Override
    public void startAndBindService(Runnable onConnected) {
        mOnConnectedRunnable = onConnected;
        Context context = ContextUtils.getApplicationContext();
        ActorForegroundServiceImpl.startActorForegroundService(context);
        Intent intent = new Intent(context, ActorForegroundService.class);
        context.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void unbindService() {
        ContextUtils.getApplicationContext().unbindService(mConnection);
        mBoundService = null;
        mOnConnectedRunnable = null;
    }

    @Override
    public boolean isConnected() {
        return mBoundService != null;
    }

    @Override
    public void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification) {
        if (mBoundService == null) {
            if (BuildConfig.ENABLE_ASSERTS) {
                Log.i(TAG, "Cannot update foreground service: not connected.");
            }
            return;
        }
        mBoundService.startOrUpdateForegroundService(
                newNotificationId, newNotification, oldNotificationId, killOldNotification);
    }

    @Override
    public void stopActorForegroundService(int flags) {
        if (mBoundService == null) {
            if (BuildConfig.ENABLE_ASSERTS) {
                Log.w(TAG, "Cannot stop foreground service: not connected.");
            }
            return;
        }
        mBoundService.stopActorForegroundService(flags);
    }

    public ServiceConnection getServiceConnectionForTesting() {
        return mConnection;
    }
}
