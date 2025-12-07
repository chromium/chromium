// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bluetooth;

import android.content.Intent;
import android.os.IBinder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManager;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManagerDelegate;

/**
 * Service that manages the Web Bluetooth notification when a website is either connected to a
 * Bluetooth device or scanning for nearby Bluetooth devices.
 */
@NullMarked
public class BluetoothNotificationServiceImpl extends SplitCompatService.Impl {
    private final BluetoothNotificationManagerDelegate mManagerDelegate =
            new BluetoothNotificationManagerDelegate() {
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

    private BluetoothNotificationManager mManager;

    @Initializer
    @Override
    public void onCreate() {
        mManager = new BluetoothNotificationManager(mManagerDelegate);
        super.onCreate();
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        mManager.onStartCommand(intent, flags, startId);
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        mManager.cancelPreviousBluetoothNotifications();
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mManager.cancelPreviousBluetoothNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return null;
    }
}
