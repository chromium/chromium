// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.nullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.res.Resources;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests that ChannelsUpdater correctly initializes channels on the notification manager. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChannelsUpdaterTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private BaseNotificationManagerProxy mNotificationManagerProxy;
    private SharedPreferencesManager mSharedPreferences;
    private ChannelsInitializer mChannelsInitializer;
    private Resources mMockResources;

    @Before
    public void setUp() {
        ChannelsInitializer.resetForTesting();
        Context context = RuntimeEnvironment.getApplication();
        mNotificationManagerProxy = BaseNotificationManagerProxyFactory.create();

        mMockResources = context.getResources();

        mChannelsInitializer =
                new ChannelsInitializer(
                        mNotificationManagerProxy,
                        ChromeChannelDefinitions.getInstance(),
                        context.getResources());
        mSharedPreferences = ChromeSharedPreferences.getInstance();

        // Delete any channels that may already have been initialized. Cleaning up here rather than
        // in tearDown in case tests running before these ones caused channels to be created.
        for (NotificationChannel channel : getChannelsIgnoringDefault()) {
            if (!channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) {
                mNotificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
    }

    @After
    public void tearDown() {
        // TODO(crbug.com/40694404): Replace with a SharedPreferencesTestRule when
        //     implemented.
        mSharedPreferences.removeKey(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION);
    }

    @Test
    public void testShouldUpdateChannels_returnsTrueIfOAndNoSavedVersionInPrefs() {
        ChannelsUpdater updater = new ChannelsUpdater(mSharedPreferences, mChannelsInitializer, 0);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    public void testShouldUpdateChannels_returnsTrueIfOAndDifferentVersionInPrefs() {
        mSharedPreferences.writeInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, 4);
        ChannelsUpdater updater = new ChannelsUpdater(mSharedPreferences, mChannelsInitializer, 5);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    public void testShouldUpdateChannels_returnsFalseIfOAndSameVersionInPrefs() {
        mSharedPreferences.writeInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, 3);
        ChannelsUpdater updater = new ChannelsUpdater(mSharedPreferences, mChannelsInitializer, 3);
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    public void testUpdateChannels_createsExpectedChannelsAndUpdatesPref() {
        ChannelsUpdater updater = new ChannelsUpdater(mSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();
        RobolectricUtil.runAllBackgroundAndUi();

        assertThat(getChannelsIgnoringDefault(), hasSize(greaterThan(0)));
        assertThat(
                getChannelIds(getChannelsIgnoringDefault()),
                containsInAnyOrder(
                        ChromeChannelDefinitions.ChannelId.BROWSER,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        ChromeChannelDefinitions.ChannelId.INCOGNITO,
                        ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK));
        assertThat(
                mSharedPreferences.readInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, -1),
                is(21));
    }

    @Test
    public void testUpdateChannels_deletesLegacyChannelsAndCreatesExpectedOnes() {
        ChromeChannelDefinitions definitions = ChromeChannelDefinitions.getInstance();
        // Set up any legacy channels.
        mNotificationManagerProxy.createNotificationChannelGroup(
                definitions
                        .getChannelGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL)
                        .toNotificationChannelGroup(mMockResources));
        for (String id : definitions.getLegacyChannelIds()) {
            NotificationChannel channel =
                    new NotificationChannel(id, id, NotificationManager.IMPORTANCE_LOW);
            channel.setGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL);
            mNotificationManagerProxy.createNotificationChannel(channel);
        }

        ChannelsUpdater updater =
                new ChannelsUpdater(
                        mSharedPreferences,
                        new ChannelsInitializer(
                                mNotificationManagerProxy, definitions, mMockResources),
                        12);
        updater.updateChannels();
        RobolectricUtil.runAllBackgroundAndUi();

        // The legacy Tips channel was enabled, so it is replaced by the Tips V2 channel.
        assertThat(
                getChannelIds(getChannelsIgnoringDefault()),
                containsInAnyOrder(
                        ChromeChannelDefinitions.ChannelId.BROWSER,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        ChromeChannelDefinitions.ChannelId.INCOGNITO,
                        ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK,
                        ChromeChannelDefinitions.ChannelId.TIPS_V2));
    }

    @Test
    public void testUpdateChannels_migratesEnabledTipsChannelBeforeStoringVersion() {
        // Simulates an upgrade from M149 or older, where the user opted in to Tips by enabling the
        // V1 Tips channel in system settings. See crbug.com/581908995.
        createTipsChannel(NotificationManager.IMPORTANCE_DEFAULT);
        ChannelsUpdater updater = createUpdaterAtVersion7(mChannelsInitializer);

        updater.updateChannels();
        // The existing channels have been read, but the background task that copies and deletes
        // the legacy channels hasn't run. An update interrupted here must be retried.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertThat(updater.shouldUpdateChannels(), is(true));

        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(findChannel(ChromeChannelDefinitions.ChannelId.TIPS), nullValue());
        assertThat(
                findChannel(ChromeChannelDefinitions.ChannelId.TIPS_V2).getImportance(),
                is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    public void testUpdateChannels_doesNotStoreVersionIfChannelsCannotBeRead() {
        // A failed read of the existing channels is reported as an empty list.
        BaseNotificationManagerProxy proxy = mock(BaseNotificationManagerProxy.class);
        doAnswer(
                        invocation -> {
                            Callback<List<NotificationChannel>> callback =
                                    invocation.getArgument(0);
                            callback.onResult(Collections.emptyList());
                            return null;
                        })
                .when(proxy)
                .getNotificationChannels(any());
        ChannelsUpdater updater =
                createUpdaterAtVersion7(
                        new ChannelsInitializer(
                                proxy, ChromeChannelDefinitions.getInstance(), mMockResources));

        updater.updateChannels();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(proxy, never()).deleteNotificationChannel(any());
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    /** Returns an updater for an install that last updated its channels in M149 or older. */
    private ChannelsUpdater createUpdaterAtVersion7(ChannelsInitializer channelsInitializer) {
        mSharedPreferences.writeInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, 7);
        return new ChannelsUpdater(
                mSharedPreferences, channelsInitializer, ChromeChannelDefinitions.CHANNELS_VERSION);
    }

    private void createTipsChannel(int importance) {
        mNotificationManagerProxy.createNotificationChannelGroup(
                ChromeChannelDefinitions.getInstance()
                        .getChannelGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL)
                        .toNotificationChannelGroup(mMockResources));
        NotificationChannel channel =
                new NotificationChannel(
                        ChromeChannelDefinitions.ChannelId.TIPS,
                        ChromeChannelDefinitions.ChannelId.TIPS,
                        importance);
        channel.setGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL);
        mNotificationManagerProxy.createNotificationChannel(channel);
    }

    private @Nullable NotificationChannel findChannel(String channelId) {
        for (NotificationChannel channel : getChannelsIgnoringDefault()) {
            if (channel.getId().equals(channelId)) return channel;
        }
        return null;
    }

    private static List<String> getChannelIds(List<NotificationChannel> channels) {
        List<String> ids = new ArrayList<>();
        for (NotificationChannel ch : channels) {
            ids.add(ch.getId());
        }
        return ids;
    }

    /**
     * Gets the current notification channels from the notification manager, except for any with the
     * default ID, which will be removed from the list before returning.
     *
     * <p>(Android *might* add a default 'Misc' channel on our behalf, but we don't want to tie our
     * tests to its presence, as this could change).
     */
    private List<NotificationChannel> getChannelsIgnoringDefault() {
        List<NotificationChannel> result = new ArrayList<>();
        mNotificationManagerProxy.getNotificationChannels(
                (channels) -> {
                    for (NotificationChannel channel : channels) {
                        if (!channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) {
                            result.add(channel);
                        }
                    }
                });

        Shadows.shadowOf(Looper.getMainLooper()).idle();
        return result;
    }
}
