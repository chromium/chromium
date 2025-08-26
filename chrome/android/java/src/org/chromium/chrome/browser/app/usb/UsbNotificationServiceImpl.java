// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.usb;

import android.content.Intent;
import android.os.IBinder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.usb.UsbNotificationManager;
import org.chromium.chrome.browser.usb.UsbNotificationManagerDelegate;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/** Service that manages the WebUSB notification when a website is connected to a USB device. */
@NullMarked
public class UsbNotificationServiceImpl extends SplitCompatService.Impl {
    private final UsbNotificationManagerDelegate mManagerDelegate =
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

    @Initializer
    @Override
    public void onCreate() {
        mManager =
                new UsbNotificationManager(
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
        mManager.cancelPreviousUsbNotifications();
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mManager.cancelPreviousUsbNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return null;
    }
}
