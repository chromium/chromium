// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Tests for {@link TwaUninstallNotificationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TwaUninstallNotificationHelperTest {
    private static final String PACKAGE_NAME = "org.chromium.test.twa";
    private static final String APP_NAME = "Test App";
    private static final String DOMAIN = "example.com";
    private static final String ORIGIN = "https://example.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private NotificationManagerProxy mNotificationManager;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManager);
    }

    @Test
    public void createNotification_withAppName() {
        Set<String> domains = new HashSet<>(Arrays.asList(DOMAIN));
        Set<String> origins = new HashSet<>(Arrays.asList(ORIGIN));

        NotificationWrapper notificationWrapper =
                TwaUninstallNotificationHelper.createNotification(
                        mContext, PACKAGE_NAME, APP_NAME, domains, origins);

        Notification notification = notificationWrapper.getNotification();
        assertNotNull(notification);
        assertEquals(ChannelId.WEBAPPS, notification.getChannelId());
        assertEquals(
                mContext.getString(R.string.twa_post_uninstall_notification_title, APP_NAME),
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        assertEquals(
                mContext.getString(R.string.twa_post_uninstall_notification_text),
                notification.extras.getCharSequence(Notification.EXTRA_TEXT).toString());

        assertEquals(PACKAGE_NAME, notificationWrapper.getMetadata().tag);
        assertEquals(PACKAGE_NAME.hashCode(), notificationWrapper.getMetadata().id);
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
                notificationWrapper.getMetadata().type);
        assertTrue((notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
    }

    @Test
    public void createNotification_nullAppName_usesDomain() {
        Set<String> domains = new HashSet<>(Arrays.asList(DOMAIN));
        Set<String> origins = new HashSet<>(Arrays.asList(ORIGIN));

        NotificationWrapper notificationWrapper =
                TwaUninstallNotificationHelper.createNotification(
                        mContext, PACKAGE_NAME, null, domains, origins);

        Notification notification = notificationWrapper.getNotification();
        assertEquals(
                mContext.getString(R.string.twa_post_uninstall_notification_title, DOMAIN),
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
    }

    @Test
    public void createNotification_emptyAppNameAndDomain_usesGenericTitle() {
        NotificationWrapper notificationWrapper =
                TwaUninstallNotificationHelper.createNotification(
                        mContext, PACKAGE_NAME, "", Collections.emptySet(), Collections.emptySet());

        Notification notification = notificationWrapper.getNotification();
        assertEquals(
                mContext.getString(R.string.twa_post_uninstall_notification_title_generic),
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
    }

    @Test
    public void showNotification_notifiesManager() {
        Set<String> domains = new HashSet<>(Arrays.asList(DOMAIN));
        Set<String> origins = new HashSet<>(Arrays.asList(ORIGIN));

        TwaUninstallNotificationHelper.showNotification(
                mContext, PACKAGE_NAME, APP_NAME, domains, origins);

        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManager).notify(captor.capture());

        NotificationWrapper wrapper = captor.getValue();
        assertEquals(PACKAGE_NAME, wrapper.getMetadata().tag);
        assertEquals(PACKAGE_NAME.hashCode(), wrapper.getMetadata().id);
    }

    @Test
    public void showNotification_emptyDomainsAndOrigins_doesNotNotify() {
        TwaUninstallNotificationHelper.showNotification(
                mContext, PACKAGE_NAME, APP_NAME, Collections.emptySet(), Collections.emptySet());

        verify(mNotificationManager, never()).notify(any());
    }
}
