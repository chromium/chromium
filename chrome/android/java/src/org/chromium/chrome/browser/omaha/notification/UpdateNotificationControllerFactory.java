// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omaha.notification;

import android.app.Activity;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * A factory that creates an {@link UpdateNotificationController} instance.
 */
public class UpdateNotificationControllerFactory {
    /**
     * @param activity Activity the notification will be shown in.
     * @param lifecycleDispatcher Lifecycle of an Activity the notification will be shown in.
     * @return a new {@link UpdateNotificationController} to use.
     */
    public static UpdateNotificationController create(
            Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.UPDATE_NOTIFICATION_SCHEDULING_INTEGRATION)) {
            return new UpdateNotificationServiceBridge(lifecycleDispatcher);
        }
        return new UpdateNotificationControllerImpl(activity, lifecycleDispatcher);
    }
}
