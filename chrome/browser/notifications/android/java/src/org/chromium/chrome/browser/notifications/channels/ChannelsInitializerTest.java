// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * Robolectric tests for ChannelsInitializer, using ChromeChannelDefinitions.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChannelsInitializerTest {
    private ChannelsInitializer mChannelsInitializer;
    private NotificationManagerProxy mNotificationManagerProxy;
    private Context mContext;
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    @Before
    @RequiresApi(Build.VERSION_CODES.O)
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> {
                    String url = inv.getArgument(0);
                    return url != null && url.contains("://")
                            ? url.substring(url.indexOf("://") + 3)
                            : url;
                });

        mContext = RuntimeEnvironment.getApplication();
        mNotificationManagerProxy = new NotificationManagerProxyImpl(mContext);
        mChannelsInitializer = new ChannelsInitializer(mNotificationManagerProxy,
                ChromeChannelDefinitions.getInstance(), mContext.getResources());

        // Delete any channels and channel groups that may already have been initialized. Cleaning
        // up here rather than in tearDown in case tests running before these ones caused channels
        // to be created.
        for (NotificationChannel channel : mNotificationManagerProxy.getNotificationChannels()) {
            if (!channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) {
                mNotificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
        for (NotificationChannelGroup group :
                mNotificationManagerProxy.getNotificationChannelGroups()) {
            mNotificationManagerProxy.deleteNotificationChannelGroup(group.getId());
        }
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testDeleteLegacyChannels_noopOnCurrentDefinitions() {
        assertThat(getChannelsIgnoringDefault(), is(empty()));

        mChannelsInitializer.deleteLegacyChannels();
        assertThat(getChannelsIgnoringDefault(), is(empty()));

        mChannelsInitializer.initializeStartupChannels();
        assertThat(getChannelsIgnoringDefault(), is(not(empty())));

        int nChannels = getChannelsIgnoringDefault().size();
        mChannelsInitializer.deleteLegacyChannels();
        assertThat(getChannelsIgnoringDefault(), hasSize(nChannels));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testInitializeStartupChannels() {
        mChannelsInitializer.initializeStartupChannels();
        List<String> notificationChannelIds = new ArrayList<>();
        for (NotificationChannel channel : getChannelsIgnoringDefault()) {
            notificationChannelIds.add(channel.getId());
        }
        assertThat(notificationChannelIds,
                containsInAnyOrder(ChromeChannelDefinitions.ChannelId.BROWSER,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        ChromeChannelDefinitions.ChannelId.INCOGNITO,
                        ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testInitializeStartupChannels_groupCreated() {
        mChannelsInitializer.initializeStartupChannels();
        assertThat(mNotificationManagerProxy.getNotificationChannelGroups(), hasSize(1));
        assertThat(mNotificationManagerProxy.getNotificationChannelGroups().get(0).getId(),
                is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testUpdateLocale_otherChannelsDoNotThrowException() {
        NotificationChannelGroup group =
                ChromeChannelDefinitions.getInstance()
                        .getChannelGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL)
                        .toNotificationChannelGroup(mContext.getResources());
        NotificationChannel channel =
                new NotificationChannel("ACCOUNT", "Account", NotificationManager.IMPORTANCE_LOW);
        channel.setGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL);
        mNotificationManagerProxy.createNotificationChannelGroup(group);
        mNotificationManagerProxy.createNotificationChannel(channel);
        mContext = RuntimeEnvironment.getApplication();
        mChannelsInitializer.updateLocale(mContext.getResources());
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_browserChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.BROWSER);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.BROWSER));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_browser)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_downloadsChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.DOWNLOADS);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.DOWNLOADS));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_downloads)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_incognitoChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.INCOGNITO);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.INCOGNITO));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_incognito)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_mediaChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_media_playback)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    @DisabledTest(message = "https://crbug.com/1201250")
    public void testEnsureInitialized_sitesChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.SITES);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));

        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.SITES));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_sites)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_contentSuggestionsDisabled() {
        // This test does not cover ensureInitialized() with ChannelId.CONTENT_SUGGESTIONS, because
        // channels ignore construction parameters when re-created. If one test created the channel
        // enabled, and the other disabled, the second test would fail.
        mChannelsInitializer.ensureInitializedAndDisabled(
                ChromeChannelDefinitions.ChannelId.CONTENT_SUGGESTIONS);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));

        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.CONTENT_SUGGESTIONS));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_content_suggestions)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_NONE));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_webappActions() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.WEBAPP_ACTIONS);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));

        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.WEBAPP_ACTIONS));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_fullscreen_controls)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_MIN));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_singleOriginSiteChannel() {
        String origin = "https://example.com";
        long creationTime = 621046800000L;
        NotificationSettingsBridge.SiteChannel siteChannel =
                SiteChannelsManager.getInstance().createSiteChannel(origin, creationTime, true);
        mChannelsInitializer.ensureInitialized(siteChannel.getId());

        assertThat(getChannelsIgnoringDefault(), hasSize(1));

        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getName().toString(), is("example.com"));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.SITES));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_multipleCalls() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.SITES);
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.BROWSER);
        assertThat(getChannelsIgnoringDefault(), hasSize(2));
    }

    @Test

    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_multipleIds() {
        Collection<String> groupIds =
                CollectionUtil.newHashSet(ChromeChannelDefinitions.ChannelGroupId.SITES,
                        ChromeChannelDefinitions.ChannelGroupId.GENERAL);
        Collection<String> channelIds =
                CollectionUtil.newHashSet(ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK,
                        ChromeChannelDefinitions.ChannelId.BROWSER);
        mChannelsInitializer.ensureInitialized(groupIds, channelIds);
        assertThat(getChannelsIgnoringDefault(), hasSize(2));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_priceDropChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.PRICE_DROP);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.PRICE_DROP));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_price_drop)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_priceDropDefaultChannel() {
        mChannelsInitializer.ensureInitialized(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_price_drop)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_securityKeyChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.SECURITY_KEY);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.SECURITY_KEY));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_security_key)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_HIGH));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_tipsChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.CHROME_TIPS);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.CHROME_TIPS));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.feature_notification_guide_notification_title)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_HIGH));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_bluetoothChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.BLUETOOTH);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.BLUETOOTH));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_bluetooth)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_usbChannel() {
        mChannelsInitializer.ensureInitialized(ChromeChannelDefinitions.ChannelId.USB);

        assertThat(getChannelsIgnoringDefault(), hasSize(1));
        NotificationChannel channel = getChannelsIgnoringDefault().get(0);
        assertThat(channel.getId(), is(ChromeChannelDefinitions.ChannelId.USB));
        assertThat(channel.getName().toString(),
                is(mContext.getString(R.string.notification_category_usb)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroup(), is(ChromeChannelDefinitions.ChannelGroupId.GENERAL));
    }

    /**
     * Gets the current notification channels from the notification manager, except for any with
     * the default ID, which will be removed from the list before returning.
     *
     * (Android *might* add a default 'Misc' channel on our behalf, but we don't want to tie our
     * tests to its presence, as this could change).
     */
    @RequiresApi(Build.VERSION_CODES.O)
    private List<NotificationChannel> getChannelsIgnoringDefault() {
        List<NotificationChannel> channels = mNotificationManagerProxy.getNotificationChannels();
        for (Iterator<NotificationChannel> it = channels.iterator(); it.hasNext();) {
            NotificationChannel channel = it.next();
            if (channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) it.remove();
        }
        return channels;
    }
}
