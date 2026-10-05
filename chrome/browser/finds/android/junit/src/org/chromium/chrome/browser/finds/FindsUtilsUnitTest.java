// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/** Unit tests for {@link FindsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FindsUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;

    @Before
    public void setUp() {
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
    }

    @Test
    public void testIsFindsChannelCreated_ChannelDoesNotExist() {
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        FindsUtils.isFindsChannelCreated(
                (channelExists) -> {
                    assertFalse(channelExists);
                });
    }

    @Test
    public void testIsFindsChannelCreated_ChannelExists() {
        NotificationChannel channel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_LOW);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(channel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        NotificationProxyUtils.setNotificationEnabledForTest(true);

        FindsUtils.isFindsChannelCreated(
                (channelExists) -> {
                    assertTrue(channelExists);
                });
    }

    @Test
    public void testIsFindsChannelCreated_AlreadyExistsButDisabled() {
        NotificationChannel channel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_NONE);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(channel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        NotificationProxyUtils.setNotificationEnabledForTest(true);

        FindsUtils.isFindsChannelCreated(
                (channelExists) -> {
                    assertTrue(channelExists);
                });
    }

    @Test
    public void testAreFindsNotificationsEnabled_AppNotificationsDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        FindsUtils.areFindsNotificationsEnabled(
                (enabled) -> {
                    assertFalse(enabled);
                });
    }

    @Test
    public void testAreFindsNotificationsEnabled_ChannelNotInitialized() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        FindsUtils.areFindsNotificationsEnabled(
                (enabled) -> {
                    assertFalse(enabled);
                });
    }

    @Test
    public void testAreFindsNotificationsEnabled_ChannelDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        NotificationChannel channel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_NONE);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(channel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        FindsUtils.areFindsNotificationsEnabled(
                (enabled) -> {
                    assertFalse(enabled);
                });
    }

    @Test
    public void testAreFindsNotificationsEnabled_Enabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        NotificationChannel channel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_LOW);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(channel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        FindsUtils.areFindsNotificationsEnabled(
                (enabled) -> {
                    assertTrue(enabled);
                });
    }

    @Test
    public void testLaunchFindsNotificationSettings_AppNotificationsEnabled() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        NotificationProxyUtils.setNotificationEnabledForTest(true);

        FindsUtils.launchFindsNotificationSettings(activity);

        Intent intent = shadowOf(activity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
        assertEquals(activity.getPackageName(), intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        assertEquals(ChannelId.CHROME_FINDS, intent.getStringExtra(Settings.EXTRA_CHANNEL_ID));
    }

    @Test
    public void testLaunchFindsNotificationSettings_AppNotificationsDisabled() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        NotificationProxyUtils.setNotificationEnabledForTest(false);

        FindsUtils.launchFindsNotificationSettings(activity);

        Intent intent = shadowOf(activity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
        assertEquals(activity.getPackageName(), intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
    }
}
