// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/**
 * A {@link android.content.BroadcastReceiver} that detects when our App level notifications are
 * blocked or unblocked via the settings menu. When this happens we record a metric.
 */
@NullMarked
public class NotificationPermissionChangeReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        if (action == null) return;

        if (action.equals(NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED)) {
            onAppNotificationStateChanged(intent);
        } else if (action.equals(
                NotificationManager.ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED)) {
            onNotificationChannelStateChanged(intent);
        }
    }

    private void onAppNotificationStateChanged(Intent intent) {
        if (!intent.hasExtra(NotificationManager.EXTRA_BLOCKED_STATE)) return;
        boolean blockedState =
                intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false);
        NotificationProxyUtils.setNotificationEnabled(!blockedState);
        NotificationUmaTracker.getInstance().onNotificationPermissionSettingChange(blockedState);
    }

    private void onNotificationChannelStateChanged(Intent intent) {
        if (!intent.hasExtra(NotificationManager.EXTRA_NOTIFICATION_CHANNEL_ID)
                || !intent.hasExtra(NotificationManager.EXTRA_BLOCKED_STATE)) {
            return;
        }

        boolean blockedState =
                intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false);

        String channelId = intent.getStringExtra(NotificationManager.EXTRA_NOTIFICATION_CHANNEL_ID);
        if (channelId == null) {
            return;
        }
        NotificationSettingsBridge.onNotificationChannelStateChanged(
                channelId, intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false));
        NotificationUmaTracker.getInstance()
                .onNotificationChannelPermissionSettingChange(channelId, blockedState);
    }
}
