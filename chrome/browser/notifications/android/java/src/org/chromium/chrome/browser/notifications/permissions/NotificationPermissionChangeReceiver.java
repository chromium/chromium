// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.notifications.NotificationUmaTracker;

/**
 * A {@link android.content.BroadcastReceiver} that detects when our App level notifications are
 * blocked or unblocked via the settings menu. When this happens we record a metric.
 */
public class NotificationPermissionChangeReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        if (action == null) return;

        if (action.equals(NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED)
                && intent.hasExtra(NotificationManager.EXTRA_BLOCKED_STATE)) {
            boolean blockedState =
                    intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false);
            NotificationUmaTracker.getInstance()
                    .onNotificationPermissionSettingChange(blockedState);
        }
    }
}
