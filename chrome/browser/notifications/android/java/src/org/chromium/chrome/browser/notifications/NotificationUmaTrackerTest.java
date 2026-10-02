// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Looper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationLifecycleEvent;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/** Unit tests for {@link NotificationUmaTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationUmaTrackerTest {
    private BaseNotificationManagerProxy mNotificationManagerProxy;
    private NotificationUmaTracker mUmaTracker;
    private ShadowNotificationManager mShadowNotificationManager;

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.getApplication();
        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        mShadowNotificationManager = Shadows.shadowOf(notificationManager);

        mNotificationManagerProxy = BaseNotificationManagerProxyFactory.create();
        mUmaTracker = NotificationUmaTracker.getInstance();

        // Clear existing channels
        mNotificationManagerProxy.getNotificationChannels(
                (channels) -> {
                    for (NotificationChannel channel : channels) {
                        mNotificationManagerProxy.deleteNotificationChannel(channel.getId());
                    }
                });
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testRecordLifecycleEvent() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Mobile.SystemNotification.Lifecycle.Sites",
                                SystemNotificationLifecycleEvent.QUEUED)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Lifecycle.Sites",
                                SystemNotificationLifecycleEvent.SHOWN)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Lifecycle.Sites",
                                SystemNotificationLifecycleEvent.CLICKED)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Lifecycle.Sites",
                                SystemNotificationLifecycleEvent.DISMISSED)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Lifecycle.BrowserActions",
                                SystemNotificationLifecycleEvent.SHOWN)
                        .build();

        mUmaTracker.recordLifecycleEvent(
                SystemNotificationType.SITES, SystemNotificationLifecycleEvent.QUEUED);
        mUmaTracker.recordLifecycleEvent(
                SystemNotificationType.SITES, SystemNotificationLifecycleEvent.SHOWN);
        mUmaTracker.recordLifecycleEvent(
                SystemNotificationType.SITES, SystemNotificationLifecycleEvent.CLICKED);
        mUmaTracker.recordLifecycleEvent(
                SystemNotificationType.SITES, SystemNotificationLifecycleEvent.DISMISSED);
        mUmaTracker.recordLifecycleEvent(
                SystemNotificationType.BROWSER_ACTIONS, SystemNotificationLifecycleEvent.SHOWN);

        watcher.assertExpected();
    }

    @Test
    public void testRecordStartupMetrics_channelStatusesAndSiteCounts() {
        // Create an ENABLED channel (e.g. BROWSER)
        NotificationChannel browserChannel =
                new NotificationChannel(
                        ChromeChannelDefinitions.ChannelId.BROWSER,
                        "Browser",
                        NotificationManager.IMPORTANCE_DEFAULT);
        mNotificationManagerProxy.createNotificationChannel(browserChannel);

        // Create a BLOCKED channel (e.g. DOWNLOADS)
        NotificationChannel downloadsChannel =
                new NotificationChannel(
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        "Downloads",
                        NotificationManager.IMPORTANCE_NONE);
        mNotificationManagerProxy.createNotificationChannel(downloadsChannel);

        // Create 2 site channels:
        // Site A: 1 enabled channel
        String siteAChannelId = SiteChannelsManager.createChannelId("https://site-a.com", 1000L);
        NotificationChannel siteAChannel =
                new NotificationChannel(
                        siteAChannelId, "site-a.com", NotificationManager.IMPORTANCE_DEFAULT);
        mNotificationManagerProxy.createNotificationChannel(siteAChannel);

        // Site B: 1 blocked channel
        String siteBChannelId = SiteChannelsManager.createChannelId("https://site-b.com", 2000L);
        NotificationChannel siteBChannel =
                new NotificationChannel(
                        siteBChannelId, "site-b.com", NotificationManager.IMPORTANCE_NONE);
        mNotificationManagerProxy.createNotificationChannel(siteBChannel);

        // Site A: duplicate older blocked channel that should be overridden by the enabled one
        String siteAOldChannelId = SiteChannelsManager.createChannelId("https://site-a.com", 500L);
        NotificationChannel siteAOldChannel =
                new NotificationChannel(
                        siteAOldChannelId, "site-a.com", NotificationManager.IMPORTANCE_NONE);
        mNotificationManagerProxy.createNotificationChannel(siteAOldChannel);

        // Set app-level notifications enabled in the shadow manager
        mShadowNotificationManager.setNotificationsEnabled(true);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Mobile.SystemNotification.Startup.AppLevelEnabled", true)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Startup.ChannelStatus.Browser",
                                SystemNotificationChannelStatus.ENABLED)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Startup.ChannelStatus.Downloads",
                                SystemNotificationChannelStatus.BLOCKED)
                        .expectIntRecord(
                                "Mobile.SystemNotification.Startup.ChannelStatus.Incognito",
                                SystemNotificationChannelStatus.UNAVAILABLE)
                        .expectIntRecord("Mobile.SystemNotification.Sites.AllowedCount", 1)
                        .expectIntRecord("Mobile.SystemNotification.Sites.DisallowedCount", 1)
                        .build();

        mUmaTracker.recordStartupMetrics();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        watcher.assertExpected();
    }
}
