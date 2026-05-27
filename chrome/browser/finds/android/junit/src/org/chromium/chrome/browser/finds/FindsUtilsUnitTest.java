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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;
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
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link FindsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FindsUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;

    @Spy private FindsService.Natives mFindsServiceNativesMock = FindsServiceJni.get();

    @Before
    public void setUp() {
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        FindsServiceJni.setInstanceForTesting(mFindsServiceNativesMock);
        UserPrefs.setPrefServiceForTesting(mPrefService);
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

    @Test
    public void testCheckShowCriteriaOptInPromo_Eligible() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        doReturn(true).when(mFindsServiceNativesMock).isHistorySyncAndMsbbEnabled(mProfile);

        when(mPrefService.getBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED))
                .thenReturn(false);
        when(mPrefService.getInteger(FindsUtils.FINDS_OPT_IN_PROMO_SHOWN_COUNT)).thenReturn(0);
        when(mPrefService.getLong(FindsUtils.FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP))
                .thenReturn(0L);

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertTrue(show);
                });
    }

    @Test
    public void testCheckShowCriteriaOptInPromo_NotificationsEnabled() {
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

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertFalse(show);
                });
    }

    @Test
    public void testCheckShowCriteriaOptInPromo_SyncDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        doReturn(false).when(mFindsServiceNativesMock).isHistorySyncAndMsbbEnabled(mProfile);

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertFalse(show);
                });
    }

    @Test
    public void testCheckShowCriteriaOptInPromo_Interacted() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        doReturn(true).when(mFindsServiceNativesMock).isHistorySyncAndMsbbEnabled(mProfile);

        when(mPrefService.getBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED))
                .thenReturn(true);

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertFalse(show);
                });
    }

    @Test
    public void testCheckShowCriteriaOptInPromo_MaxShowCountReached() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        doReturn(true).when(mFindsServiceNativesMock).isHistorySyncAndMsbbEnabled(mProfile);

        when(mPrefService.getBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED))
                .thenReturn(false);
        when(mPrefService.getInteger(FindsUtils.FINDS_OPT_IN_PROMO_SHOWN_COUNT)).thenReturn(3);

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertFalse(show);
                });
    }

    @Test
    public void testCheckShowCriteriaOptInPromo_UnderCooldown() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        doReturn(true).when(mFindsServiceNativesMock).isHistorySyncAndMsbbEnabled(mProfile);

        when(mPrefService.getBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED))
                .thenReturn(false);
        when(mPrefService.getInteger(FindsUtils.FINDS_OPT_IN_PROMO_SHOWN_COUNT)).thenReturn(0);
        when(mPrefService.getLong(FindsUtils.FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP))
                .thenReturn(System.currentTimeMillis());

        FindsUtils.checkShowCriteriaOptInPromo(
                mProfile,
                (show) -> {
                    assertFalse(show);
                });
    }
}
