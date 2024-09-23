// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bluetooth;

import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManager;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManagerDelegate;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/**
 * Service that manages the Web Bluetooth notification when a website is either connected
 * to a Bluetooth device or scanning for nearby Bluetooth devices.
 */
public class BluetoothNotificationServiceImpl extends BluetoothNotificationService.Impl {
    private BluetoothNotificationManagerDelegate mManagerDelegate =
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

    @Override
    public void onCreate() {
        mManager =
                new BluetoothNotificationManager(
                        BaseNotificationManagerProxyFactory.create(
                                ContextUtils.getApplicationContext()),
                        mManagerDelegate);
        super.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
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
    public IBinder onBind(Intent intent) {
        return null;
    }
}
