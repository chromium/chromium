// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.usb;

import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.usb.UsbNotificationManager;
import org.chromium.chrome.browser.usb.UsbNotificationManagerDelegate;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/**
 * Service that manages the WebUSB notification when a website is connected
 * to a USB device.
 */
public class UsbNotificationServiceImpl extends UsbNotificationService.Impl {
    private UsbNotificationManagerDelegate mManagerDelegate =
            new UsbNotificationManagerDelegate() {
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

    private UsbNotificationManager mManager;

    @Override
    public void onCreate() {
        mManager =
                new UsbNotificationManager(
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
        mManager.cancelPreviousUsbNotifications();
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mManager.cancelPreviousUsbNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
