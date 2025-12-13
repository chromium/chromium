// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.embedder_support.util.Origin;

/** Tests for {@link NotificationChannelPreserverTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationChannelPreserverTest {
    private static final Origin ORIGIN_WITH_CHANNEL = Origin.create("https://www.red.com");
    private static final String CHANNEL_ID = "red-channel-id";
    private static final Origin ORIGIN_WITHOUT_CHANNEL = Origin.create("https://www.blue.com");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock InstalledWebappPermissionStore mStore;
    @Mock SiteChannelsManager mSiteChannelsManager;

    @Before
    public void setUp() {

        SiteChannelsManager.setInstanceForTesting(mSiteChannelsManager);
        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);

        doAnswer(
                        (invocation) -> {
                            Callback<String> callback = invocation.getArgument(1);
                            callback.onResult(CHANNEL_ID);
                            return null;
                        })
                .when(mSiteChannelsManager)
                .getChannelIdForOriginAsync(
                        eq(ORIGIN_WITH_CHANNEL.toString()), any(Callback.class));
        doAnswer(
                        (invocation) -> {
                            Callback<String> callback = invocation.getArgument(1);
                            callback.onResult(ChromeChannelDefinitions.ChannelId.SITES);
                            return null;
                        })
                .when(mSiteChannelsManager)
                .getChannelIdForOriginAsync(
                        eq(ORIGIN_WITHOUT_CHANNEL.toString()), any(Callback.class));
    }

    @Test
    public void delete_savesOldValueEnabled() {
        testSaveOldValue(true);
    }

    @Test
    public void delete_savesOldValueDisabled() {
        testSaveOldValue(false);
    }

    private void testSaveOldValue(boolean enabled) {
        setChannelStatus(enabled);

        NotificationChannelPreserver.deleteChannelIfNeeded(ORIGIN_WITH_CHANNEL);

        @ContentSetting int settingValue = enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK;
        verify(mStore)
                .setPreInstallNotificationPermission(eq(ORIGIN_WITH_CHANNEL), eq(settingValue));
        verify(mSiteChannelsManager).deleteSiteChannel(eq(CHANNEL_ID));
    }

    @Test
    public void delete_nopIfNoChannel() {
        NotificationChannelPreserver.deleteChannelIfNeeded(ORIGIN_WITHOUT_CHANNEL);

        verify(mStore, never()).setPreInstallNotificationPermission(any(), anyInt());
        verify(mSiteChannelsManager, never()).deleteSiteChannel(any());
    }

    @Test
    public void restore_nopIfNoStore() {
        setPreInstallNotificationPermission(ORIGIN_WITHOUT_CHANNEL, null);
        NotificationChannelPreserver.restoreChannelIfNeeded(ORIGIN_WITHOUT_CHANNEL);
        verify(mSiteChannelsManager, never()).createSiteChannel(any(), anyLong(), anyBoolean());
    }

    @Test
    public void restore_createsChannelEnabled() {
        testCreatesChannel(true);
    }

    @Test
    public void restore_createsChannelDisabled() {
        testCreatesChannel(false);
    }

    private void testCreatesChannel(boolean enabled) {
        @ContentSetting int settingValue = enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK;
        setPreInstallNotificationPermission(ORIGIN_WITH_CHANNEL, settingValue);
        NotificationChannelPreserver.restoreChannelIfNeeded(ORIGIN_WITH_CHANNEL);
        verify(mSiteChannelsManager)
                .createSiteChannel(eq(ORIGIN_WITH_CHANNEL.toString()), anyLong(), eq(enabled));
    }

    private void setChannelStatus(boolean enabled) {
        doAnswer(
                        new Answer<>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) throws Throwable {
                                String channelIdArg = invocation.getArgument(0);
                                Callback<Integer> callbackArg = invocation.getArgument(1);

                                if (channelIdArg.equals(CHANNEL_ID)) {
                                    if (enabled) {
                                        callbackArg.onResult(NotificationChannelStatus.ENABLED);
                                    } else {
                                        callbackArg.onResult(NotificationChannelStatus.BLOCKED);
                                    }
                                }
                                return null; // Method is void
                            }
                        })
                .when(mSiteChannelsManager)
                .getChannelStatusAsync(eq(CHANNEL_ID), any(Callback.class));
    }

    private void setPreInstallNotificationPermission(
            Origin origin, @ContentSetting Integer settingValue) {
        when(mStore.getAndRemovePreInstallNotificationPermission(origin)).thenReturn(settingValue);
    }
}
