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
import static org.junit.Assert.fail;

import android.app.NotificationChannel;
import android.app.NotificationManager;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge.SiteChannel;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager.ChannelMigrationResult;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

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
@Batch(Batch.PER_CLASS)
public class SiteChannelsManagerTest {
    private SiteChannelsManager mSiteChannelsManager;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        clearExistingSiteChannels(NotificationManagerProxyImpl.getInstance());
        mSiteChannelsManager = SiteChannelsManager.getInstance();
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

    private List<SiteChannel> getSiteChannels() {
        final List<SiteChannel> result = new ArrayList<>();
        CallbackHelper callbackHelper = new CallbackHelper();
        mSiteChannelsManager.getSiteChannelsAsync(
                (siteChannels) -> {
                    Collections.addAll(result, siteChannels);
                    callbackHelper.notifyCalled();
                });

        try {
            callbackHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail("Callback timed out: " + e.getMessage());
        }
        return result;
    }

    @Test
    @SmallTest
    public void testCreateSiteChannel_enabled() {
        mSiteChannelsManager.createSiteChannel("https://example-enabled.org", 62102180000L, true);
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = siteChannels.get(0);
        assertThat(channel.getOrigin(), is("https://example-enabled.org"));
        assertThat(channel.getStatus(), matchesChannelStatus(NotificationChannelStatus.ENABLED));
        assertThat(channel.getTimestamp(), is(62102180000L));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testCreateSiteChannel_highPriorityEnabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSiteChannelsManager.createSiteChannel("https://example-highpri.org", 62102180000L, true);
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = siteChannels.get(0);
        assertThat(channel.toChannel().getImportance(), is(NotificationManager.IMPORTANCE_HIGH));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testCreateSiteChannel_highPriorityEnabled_nonDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        mSiteChannelsManager.createSiteChannel(
                "https://example-nondesktop.org", 62102180000L, true);
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = siteChannels.get(0);
        assertThat(channel.toChannel().getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testCreateSiteChannel_highPriorityDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSiteChannelsManager.createSiteChannel(
                "https://example-defaultpri.org", 62102180000L, true);
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = siteChannels.get(0);
        assertThat(channel.toChannel().getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
    }

    @Test
    @SmallTest
    public void testCreateSiteChannel_stripsSchemaForChannelName() {
        mSiteChannelsManager.createSiteChannel("http://127.0.0.1", 0L, true);
        mSiteChannelsManager.createSiteChannel("https://example.com", 0L, true);
        mSiteChannelsManager.createSiteChannel("ftp://127.0.0.1", 0L, true);
        List<String> channelNames = new ArrayList<>();
        List<SiteChannel> siteChannels = getSiteChannels();
        for (NotificationSettingsBridge.SiteChannel siteChannel : siteChannels) {
            channelNames.add(siteChannel.toChannel().getName().toString());
        }
        assertThat(channelNames, containsInAnyOrder("ftp://127.0.0.1", "example.com", "127.0.0.1"));
    }

    @Test
    @SmallTest
    public void testCreateSiteChannel_disabled() {
        mSiteChannelsManager.createSiteChannel("https://example-blocked.org", 0L, false);
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = siteChannels.get(0);
        assertThat(channel.getOrigin(), is("https://example-blocked.org"));
        assertThat(channel.getStatus(), matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @SmallTest
    public void testDeleteSiteChannel_channelExists() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel(channel.getId());
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(0));
    }

    @Test
    @SmallTest
    public void testDeleteAllSiteChannels() {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.createSiteChannel("https://tests.peter.sh", 0L, true);
        mSiteChannelsManager.deleteAllSiteChannels();
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(0));
    }

    @Test
    @SmallTest
    public void testDeleteSiteChannel_channelDoesNotExist() {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel("https://some-other-origin.org");
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
    }

    @Test
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsEnabled() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://example-enabled.org", 0L, true);
        assertThat(
                getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.ENABLED));
    }

    @Test
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsBlocked() {
        assertThat(
                getChannelStatus("https://example-blocked.com"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://example-blocked.com", 0L, false);
        assertThat(
                getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @SmallTest
    public void testGetChannelStatus_channelNotCreated() {
        assertThat(
                getChannelStatus("invalid-channel-id"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    @Test
    @SmallTest
    public void testGetChannelStatus_channelCreatedThenDeleted() {
        NotificationSettingsBridge.SiteChannel channel =
                mSiteChannelsManager.createSiteChannel("https://chromium.org", 0L, true);
        mSiteChannelsManager.deleteSiteChannel(channel.getId());
        assertThat(
                getChannelStatus(channel.getId()),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    @Test
    @SmallTest
    public void testBlockingPermissionInIncognitoTabbedActivityCreatesNoChannels() {
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.NOTIFICATIONS,
                        "https://example-incognito.com",
                        null,
                        /* isEmbargoed= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    info.setContentSetting(
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOtrProfile(/* createIfNeeded= */ true),
                            ContentSetting.BLOCK);
                });
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(0));
    }

    @Test
    @SmallTest
    public void testBlockingPermissionInIncognitoCctCreatesNoChannels() {
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.NOTIFICATIONS,
                        "https://example-incognito.com",
                        null,
                        /* isEmbargoed= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtrProfileId otrProfileId = OtrProfileId.createUnique("CCT:Incognito");
                    Profile nonPrimaryOtrProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileId, /* createIfNeeded= */ true);
                    assertNotNull(nonPrimaryOtrProfile);
                    assertTrue(nonPrimaryOtrProfile.isOffTheRecord());
                    info.setContentSetting(nonPrimaryOtrProfile, ContentSetting.BLOCK);
                });
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(0));
    }

    private static Matcher<Integer> matchesChannelStatus(
            @NotificationChannelStatus final int status) {
        return new BaseMatcher<>() {
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
    @MediumTest
    public void testGetChannelIdForOrigin_unknownOrigin() {
        CallbackHelper callbackHelper = new CallbackHelper();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Notifications.Android.SitesChannel", true);
        mSiteChannelsManager.getChannelIdForOriginAsync(
                "https://unknown.com",
                (channelId) -> {
                    assertThat(channelId, is(ChromeChannelDefinitions.ChannelId.SITES));
                    callbackHelper.notifyCalled();
                });

        try {
            callbackHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail("Callback timed out: " + e.getMessage());
        }

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testMigrateSiteChannel_upgradeToHighPriority() {
        DeviceInfo.setIsDesktopForTesting(false);
        SiteChannel initialChannel =
                mSiteChannelsManager.createSiteChannel("https://upgrade-test.org", 1000L, true);
        assertThat(initialChannel.getId().startsWith("web:"), is(true));
        assertThat(
                initialChannel.toChannel().getImportance(),
                is(NotificationManager.IMPORTANCE_DEFAULT));

        DeviceInfo.setIsDesktopForTesting(true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Android.ChannelMigration.Result",
                        ChannelMigrationResult.MIGRATED_TO_HIGH_PRIORITY);
        String migratedChannelId = getChannelIdForOrigin("https://upgrade-test.org");

        assertThat(migratedChannelId.startsWith("web_high:"), is(true));
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        SiteChannel migratedChannel = siteChannels.get(0);
        assertThat(migratedChannel.getId(), is(migratedChannelId));
        assertThat(
                migratedChannel.toChannel().getImportance(),
                is(NotificationManager.IMPORTANCE_HIGH));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testMigrateSiteChannel_downgradeToDefault() {
        DeviceInfo.setIsDesktopForTesting(true);
        SiteChannel initialChannel =
                mSiteChannelsManager.createSiteChannel("https://downgrade-test.org", 2000L, true);
        assertThat(initialChannel.getId().startsWith("web_high:"), is(true));
        assertThat(
                initialChannel.toChannel().getImportance(),
                is(NotificationManager.IMPORTANCE_HIGH));

        DeviceInfo.setIsDesktopForTesting(false);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Android.ChannelMigration.Result",
                        ChannelMigrationResult.MIGRATED_TO_DEFAULT_PRIORITY);
        String migratedChannelId = getChannelIdForOrigin("https://downgrade-test.org");

        assertThat(migratedChannelId.startsWith("web:"), is(true));
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        SiteChannel migratedChannel = siteChannels.get(0);
        assertThat(migratedChannel.getId(), is(migratedChannelId));
        assertThat(
                migratedChannel.toChannel().getImportance(),
                is(NotificationManager.IMPORTANCE_DEFAULT));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testMigrateSiteChannel_noMigrationNeeded_alreadyHighPriority() {
        DeviceInfo.setIsDesktopForTesting(true);
        SiteChannel initialChannel =
                mSiteChannelsManager.createSiteChannel(
                        "https://steady-state-high.org", 4000L, true);
        assertThat(initialChannel.getId().startsWith("web_high:"), is(true));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Android.ChannelMigration.Result",
                        ChannelMigrationResult.NO_MIGRATION_NEEDED);
        String channelId = getChannelIdForOrigin("https://steady-state-high.org");

        assertThat(channelId, is(initialChannel.getId()));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testMigrateSiteChannel_noMigrationNeeded_alreadyDefaultPriority() {
        DeviceInfo.setIsDesktopForTesting(false);
        SiteChannel initialChannel =
                mSiteChannelsManager.createSiteChannel(
                        "https://steady-state-default.org", 5000L, true);
        assertThat(initialChannel.getId().startsWith("web:"), is(true));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Android.ChannelMigration.Result",
                        ChannelMigrationResult.NO_MIGRATION_NEEDED);
        String channelId = getChannelIdForOrigin("https://steady-state-default.org");

        assertThat(channelId, is(initialChannel.getId()));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
    public void testMigrateSiteChannel_preservesBlockedStatus() {
        DeviceInfo.setIsDesktopForTesting(false);
        SiteChannel initialChannel =
                mSiteChannelsManager.createSiteChannel("https://blocked-migrate.org", 3000L, false);
        assertThat(initialChannel.getId().startsWith("web:"), is(true));
        assertThat(
                getChannelStatus(initialChannel.getId()),
                matchesChannelStatus(NotificationChannelStatus.BLOCKED));

        DeviceInfo.setIsDesktopForTesting(true);
        String migratedChannelId = getChannelIdForOrigin("https://blocked-migrate.org");

        assertThat(migratedChannelId.startsWith("web_high:"), is(true));
        List<SiteChannel> siteChannels = getSiteChannels();
        assertThat(siteChannels, hasSize(1));
        assertThat(
                getChannelStatus(migratedChannelId),
                matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    private String getChannelIdForOrigin(String origin) {
        PayloadCallbackHelper<String> helper = new PayloadCallbackHelper<>();
        mSiteChannelsManager.getChannelIdForOriginAsync(origin, helper::notifyCalled);
        return helper.getOnlyPayloadBlocking();
    }

    private static @NotificationChannelStatus int getChannelStatus(String channelId) {
        PayloadCallbackHelper<Integer> helper = new PayloadCallbackHelper<>();
        SiteChannelsManager.getInstance().getChannelStatusAsync(channelId, helper::notifyCalled);
        return helper.getOnlyPayloadBlocking();
    }
}
