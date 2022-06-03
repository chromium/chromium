// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Tests for {@link NotificationChannelPreserverTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O )
public class NotificationChannelPreserverTest {
    private static final Origin ORIGIN_WITH_CHANNEL = Origin.create("https://www.red.com");
    private static final String CHANNEL_ID = "red-channel-id";
    private static final Origin ORIGIN_WITHOUT_CHANNEL = Origin.create("https://www.blue.com");

    @Mock TrustedWebActivityPermissionStore mStore;
    @Mock SiteChannelsManager mSiteChannelsManager;

    private NotificationChannelPreserver mPreserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPreserver = new NotificationChannelPreserver(mStore, mSiteChannelsManager);

        when(mSiteChannelsManager.getChannelIdForOrigin(eq(ORIGIN_WITH_CHANNEL.toString())))
                .thenReturn(CHANNEL_ID);
        when(mSiteChannelsManager.getChannelIdForOrigin(eq(ORIGIN_WITHOUT_CHANNEL.toString())))
                .thenReturn(ChromeChannelDefinitions.ChannelId.SITES);
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

        mPreserver.deleteChannel(ORIGIN_WITH_CHANNEL);

        verify(mStore).setPreTwaNotificationState(eq(ORIGIN_WITH_CHANNEL), eq(enabled));
        verify(mSiteChannelsManager).deleteSiteChannel(eq(CHANNEL_ID));
    }

    @Test
    public void delete_nopIfNoChannel() {
        mPreserver.deleteChannel(ORIGIN_WITHOUT_CHANNEL);

        verify(mStore, never()).setPreTwaNotificationState(any(), anyBoolean());
        verify(mSiteChannelsManager, never()).deleteSiteChannel(any());
    }

    @Test
    public void restore_nopIfNoStore() {
        setPreTwaChannelStatus(ORIGIN_WITHOUT_CHANNEL,null);
        mPreserver.restoreChannel(ORIGIN_WITHOUT_CHANNEL);
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
        setPreTwaChannelStatus(ORIGIN_WITH_CHANNEL,enabled);
        mPreserver.restoreChannel(ORIGIN_WITH_CHANNEL);
        verify(mSiteChannelsManager)
                .createSiteChannel(eq(ORIGIN_WITH_CHANNEL.toString()), anyLong(), eq(enabled));
    }

    private void setChannelStatus(boolean enabled) {
        when(mSiteChannelsManager.getChannelStatus(eq(CHANNEL_ID))).thenReturn(
                enabled ? NotificationChannelStatus.ENABLED : NotificationChannelStatus.BLOCKED);
    }

    private void setPreTwaChannelStatus(Origin origin, Boolean value) {
        when(mStore.getPreTwaNotificationState(origin)).thenReturn(value);
    }
}
