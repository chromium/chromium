// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.support.v4.app.NotificationManagerCompat;

import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Enables custom implementation for the notification site settings category, similar to
 * {@link LocationCategory}.
 */
public class NotificationCategory extends SiteSettingsCategory {
    NotificationCategory() {
        // Android does not treat notifications as a 'permission', i.e. notification status cannot
        // be checked via Context#checkPermission(). Hence we pass an empty string here and override
        // #enabledForChrome() to use the notification-status checking API instead.
        super(Type.NOTIFICATIONS, "" /* androidPermission*/);
    }

    @Override
    protected boolean enabledForChrome(Context context) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.APP_NOTIFICATION_STATUS_MESSAGING)) {
            return super.enabledForChrome(context);
        }
        NotificationManagerCompat manager = NotificationManagerCompat.from(context);
        return manager.areNotificationsEnabled();
    }
}
