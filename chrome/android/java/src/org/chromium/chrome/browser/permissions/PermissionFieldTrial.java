// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.v4.app.NotificationCompat;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides Field Trial support for the permissions field trial
 */
public class PermissionFieldTrial {
    // Keep in sync with "chrome/browser/permissions/permission_features.h"
    private static final String QUIET_NOTIFICATION_PROMPTS_UI_FLAVOR_PARAMETER_NAME = "ui_flavour";
    private static final String QUIET_NOTIFICATION_PROMPTS_HEADS_UP_NOTIFICATION =
            "heads_up_notification";
    private static final String QUIET_NOTIFICATION_PROMPTS_MINI_INFOBAR = "mini_infobar";

    @IntDef({UIFlavor.NONE, UIFlavor.QUIET_NOTIFICATION, UIFlavor.HEADS_UP_NOTIFICATION,
            UIFlavor.MINI_INFOBAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UIFlavor {
        int NONE = 0;
        int QUIET_NOTIFICATION = 1;
        int HEADS_UP_NOTIFICATION = 2;
        int MINI_INFOBAR = 3;
    }

    public static @UIFlavor int uiFlavorToUse() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS)) {
            return UIFlavor.NONE;
        }

        switch (ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS,
                PermissionFieldTrial.QUIET_NOTIFICATION_PROMPTS_UI_FLAVOR_PARAMETER_NAME)) {
            case PermissionFieldTrial.QUIET_NOTIFICATION_PROMPTS_HEADS_UP_NOTIFICATION:
                return UIFlavor.HEADS_UP_NOTIFICATION;
            case PermissionFieldTrial.QUIET_NOTIFICATION_PROMPTS_MINI_INFOBAR:
                return UIFlavor.MINI_INFOBAR;
            default:
                return UIFlavor.QUIET_NOTIFICATION;
        }
    }

    public static @ChannelDefinitions.ChannelId String notificationChannelIdToUse() {
        switch (uiFlavorToUse()) {
            case UIFlavor.QUIET_NOTIFICATION:
                return ChannelDefinitions.ChannelId.PERMISSION_REQUESTS;
            case UIFlavor.HEADS_UP_NOTIFICATION:
                return ChannelDefinitions.ChannelId.PERMISSION_REQUESTS_HIGH;
            default:
                return ChannelDefinitions.ChannelId.BROWSER;
        }
    }

    public static int notificationPriorityToUse() {
        if (uiFlavorToUse() == UIFlavor.HEADS_UP_NOTIFICATION) {
            return NotificationCompat.PRIORITY_MAX;
        }

        return NotificationCompat.PRIORITY_LOW;
    }

    public static @NotificationUmaTracker.SystemNotificationType int systemNotificationTypeToUse() {
        switch (uiFlavorToUse()) {
            case UIFlavor.QUIET_NOTIFICATION:
                return NotificationUmaTracker.SystemNotificationType.PERMISSION_REQUESTS;
            case UIFlavor.HEADS_UP_NOTIFICATION:
                return NotificationUmaTracker.SystemNotificationType.PERMISSION_REQUESTS_HIGH;
            default:
                return NotificationUmaTracker.SystemNotificationType.UNKNOWN;
        }
    }
}
