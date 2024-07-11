// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.NotificationChannel;
import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation unit tests for SiteChannelsManager.
 *
 * <p>They run against the real NotificationManager so we can be sure Android does what we expect.
 *
 * <p>Note that channels are persistent by Android so even if a channel is deleted, if it is
 * recreated with the same id then the previous properties will be restored, including whether the
 * channel was blocked. Thus some of these tests use different channel ids to avoid this problem.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@RequiresApi(Build.VERSION_CODES.O)
public class SiteChannelsManagerTest {
    private SiteChannelsManager mSiteChannelsManager;
    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        Context mContext = ApplicationProvider.getApplicationContext();
        NotificationManagerProxy notificationManagerProxy =
                new NotificationManagerProxyImpl(mContext);
        clearExistingSiteChannels(notificationManagerProxy);
        mSiteChannelsManager = new SiteChannelsManager(notificationManagerProxy);
    }

    private static void clearExistingSiteChannels(
            NotificationManagerProxy notificationManagerProxy) {
        for (NotificationChannel channel : notificationManagerProxy.getNotificationChannels()) {
            if (SiteChannelsManager.isValidSiteChannelId(channel.getId())
                    || (channel.getGroup() != null
                            && channel.getGroup()
                                    .equals(ChromeChannelDefinitions.ChannelGroupId.SITES))) {
                notificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testCreateSiteChannel_enabled() {
        mSiteChannelsManager.createSiteChannel("https://example-enabled.org", 62102180000L, true);
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = mSiteChannelsManager.getSiteChannels()[0];
        assertThat(channel.getOrigin(), is("https://example-enabled.org"));
        assertThat(channel.getStatus(), matchesChannelStatus(NotificationChannelStatus.ENABLED));
        assertThat(channel.getTimestamp(), is(62102180000L));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testCreateSiteChannel_stripsSchemaForChannelName() {
        mSiteChannelsManager.createSiteChannel("http://127.0.0.1", 0L, true);
        mSiteChannelsManager.createSiteChannel("https://example.com", 0L, true);
        mSiteChannelsManager.createSiteChannel("ftp://127.0.0.1", 0L, true);
        List<String> channelNames = new ArrayList<>();
        for (NotificationSettingsBridge.SiteChannel siteChannel :
                mSiteChannelsManager.getSiteChannels()) {
            channelNames.add(siteChannel.toChannel().getName().toString());
        }
        assertThat(channelNames, containsInAnyOrder("ftp://127.0.0.1", "example.com", "127.0.0.1"));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @SmallTest
    public void testCreateSiteChannel_disabled() {
        mSiteChannelsManager.createSiteChannel("https://example-blocked.org", 0L, false);
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = mSiteChannelsManager.getSiteChannels()[0];
        assertThat(channel.getOrigin(), is("https://example-blocked.org"));
        assertThat(channel.getStatus(), matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testDeleteSiteChannel_channelExists() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel(channel.getId());
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(0));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testDeleteAllSiteChannels() {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.createSiteChannel("https://tests.peter.sh", 0L, true);
        mSiteChannelsManager.deleteAllSiteChannels();
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(0));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testDeleteSiteChannel_channelDoesNotExist() {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel("https://some-other-origin.org");
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsEnabled() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://example-enabled.org", 0L, true);
        assertThat(
                mSiteChannelsManager.getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.ENABLED));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsBlocked() {
        assertThat(
                mSiteChannelsManager.getChannelStatus("https://example-blocked.com"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://example-blocked.com", 0L, false);
        assertThat(
                mSiteChannelsManager.getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testGetChannelStatus_channelNotCreated() {
        assertThat(
                mSiteChannelsManager.getChannelStatus("invalid-channel-id"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testGetChannelStatus_channelCreatedThenDeleted() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel(channel.getId());
        assertThat(
                mSiteChannelsManager.getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testBlockingPermissionInIncognitoTabbedActivityCreatesNoChannels() {
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.NOTIFICATIONS,
                        "https://example-incognito.com",
                        null,
                        /* isEmbargo= */ true,
                        SessionModel.DURABLE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    info.setContentSetting(
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOTRProfile(/* createIfNeeded= */ true),
                            ContentSettingValues.BLOCK);
                });
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(0));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @SmallTest
    public void testBlockingPermissionInIncognitoCCTCreatesNoChannels() {
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.NOTIFICATIONS,
                        "https://example-incognito.com",
                        null,
                        /* isEmbargo= */ true,
                        SessionModel.DURABLE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
                    Profile nonPrimaryOTRProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                    assertNotNull(nonPrimaryOTRProfile);
                    assertTrue(nonPrimaryOTRProfile.isOffTheRecord());
                    info.setContentSetting(nonPrimaryOTRProfile, ContentSettingValues.BLOCK);
                });
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(0));
    }

    private static Matcher<Integer> matchesChannelStatus(
            @NotificationChannelStatus final int status) {
        return new BaseMatcher<Integer>() {
            @Override
            public boolean matches(Object o) {
                return status == (int) o;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("NotificationChannelStatus." + statusToString(status));
            }

            @Override
            public void describeMismatch(final Object item, final Description description) {
                description.appendText(
                        "was NotificationChannelStatus." + statusToString((int) item));
            }
        };
    }

    private static String statusToString(@NotificationChannelStatus int status) {
        switch (status) {
            case NotificationChannelStatus.ENABLED:
                return "ENABLED";
            case NotificationChannelStatus.BLOCKED:
                return "BLOCKED";
            case NotificationChannelStatus.UNAVAILABLE:
                return "UNAVAILABLE";
        }
        return null;
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @MediumTest
    public void testGetChannelIdForOrigin_unknownOrigin() {
        String channelId = mSiteChannelsManager.getChannelIdForOrigin("https://unknown.com");

        assertThat(channelId, is(ChromeChannelDefinitions.ChannelId.SITES));

        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Notifications.Android.SitesChannel"),
                is(1));
    }
}
