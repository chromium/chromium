// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.send_tab_to_self;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.share.send_tab_to_self.NotificationManager;

/** Handles changes to notifications based on user action or timeout. */
public class SendTabToSelfNotificationReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        NotificationManager.handleIntent(intent);
                    }
                };

        // Try to load native.
        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }

    @CalledByNative
    public static Class<?> getSendTabToSelfNotificationReciever() {
        return SendTabToSelfNotificationReceiver.class;
    }
}
