// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import static org.junit.Assert.assertEquals;

import android.app.NotificationManager;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;

/** Robolectric tests for {@link NotificationPermissionChangeReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, manifest = Config.NONE)
public class NotificationPermissionChangeReceiverTest {

    private void verifyPermissionChangeHistogramWasRecorded(boolean expectedPermissionState) {
        int histogramValue = expectedPermissionState ? 1 : 0;

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.Change", histogramValue));
    }

    @Test
    public void blockingNotificationsShouldRecordAHistogram() {
        NotificationPermissionChangeReceiver receiver = new NotificationPermissionChangeReceiver();

        // Broadcast sent by Android when the user changes the app's notification state.
        Intent broadcastIntent = new Intent(NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED);
        // Extra indicating that notifications are now blocked.
        broadcastIntent.putExtra(NotificationManager.EXTRA_BLOCKED_STATE, true);

        receiver.onReceive(ApplicationProvider.getApplicationContext(), broadcastIntent);

        verifyPermissionChangeHistogramWasRecorded(false);
    }

    @Test
    public void unblockingNotificationsShouldRecordAHistogram() {
        NotificationPermissionChangeReceiver receiver = new NotificationPermissionChangeReceiver();

        // Broadcast sent by Android when the user changes the app's notification state.
        Intent broadcastIntent = new Intent(NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED);
        // Extra indicating that notifications are now unblocked.
        broadcastIntent.putExtra(NotificationManager.EXTRA_BLOCKED_STATE, false);

        receiver.onReceive(ApplicationProvider.getApplicationContext(), broadcastIntent);

        verifyPermissionChangeHistogramWasRecorded(true);
    }

    @Test
    public void testChannelBlocked_RecordsHistogram() {
        NotificationPermissionChangeReceiver receiver = new NotificationPermissionChangeReceiver();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Mobile.SystemNotification.Permission.Change.Tips", false);

        // Broadcast sent by Android when the user changes a notification channel's state.
        Intent broadcastIntent =
                new Intent(NotificationManager.ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED);
        broadcastIntent.putExtra(
                NotificationManager.EXTRA_NOTIFICATION_CHANNEL_ID,
                ChromeChannelDefinitions.ChannelId.TIPS);
        broadcastIntent.putExtra(NotificationManager.EXTRA_BLOCKED_STATE, true);

        receiver.onReceive(ApplicationProvider.getApplicationContext(), broadcastIntent);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testChannelUnblocked_RecordsHistogram() {
        NotificationPermissionChangeReceiver receiver = new NotificationPermissionChangeReceiver();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Mobile.SystemNotification.Permission.Change.Tips", true);

        // Broadcast sent by Android when the user changes a notification channel's state.
        Intent broadcastIntent =
                new Intent(NotificationManager.ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED);
        broadcastIntent.putExtra(
                NotificationManager.EXTRA_NOTIFICATION_CHANNEL_ID,
                ChromeChannelDefinitions.ChannelId.TIPS);
        broadcastIntent.putExtra(NotificationManager.EXTRA_BLOCKED_STATE, false);

        receiver.onReceive(ApplicationProvider.getApplicationContext(), broadcastIntent);
        histogramWatcher.assertExpected();
    }
}
