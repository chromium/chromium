// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.serial;

import android.content.Intent;
import android.os.IBinder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.serial.SerialNotificationManager;
import org.chromium.chrome.browser.serial.SerialNotificationManagerDelegate;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/** Service that manages the WebSerial notification when a website is connected to a serial port. */
@NullMarked
public class SerialNotificationServiceImpl extends SplitCompatService.Impl {
    private final SerialNotificationManagerDelegate mManagerDelegate =
            new SerialNotificationManagerDelegate() {
                @Override
                public Intent createTrustedBringTabToFrontIntent(int tabId) {
                    return IntentHandler.createTrustedBringTabToFrontIntent(
                            tabId, IntentHandler.BringToFrontSource.NOTIFICATION);
                }

                @Override
                public void stopSelf() {
                    getService().stopSelf();
                }

                @Override
                public void stopSelf(int startId) {
                    getService().stopSelf(startId);
                }
            };

    private SerialNotificationManager mManager;

    @Override
    public void onCreate() {
        mManager =
                new SerialNotificationManager(
                        BaseNotificationManagerProxyFactory.create(), mManagerDelegate);
        super.onCreate();
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        mManager.onStartCommand(intent, flags, startId);
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        mManager.cancelPreviousSerialNotifications();
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mManager.cancelPreviousSerialNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return null;
    }
}
