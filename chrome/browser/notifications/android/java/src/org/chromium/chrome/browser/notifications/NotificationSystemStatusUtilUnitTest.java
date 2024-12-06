// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/**
 * JUnit tests for NotificationSystemStatusUtil which run against Robolectric so that we can
 * manipulate what NotificationManagerCompat.getNotificationsEnabled returns.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationSystemStatusUtilUnitTest {
    @Test
    public void testAppNotificationStatusEnabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        assertThat(
                NotificationSystemStatusUtil.getAppNotificationStatus(),
                is(NotificationSystemStatusUtil.APP_NOTIFICATIONS_STATUS_ENABLED));
    }

    @Test
    public void testAppNotificationStatusDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        assertThat(
                NotificationSystemStatusUtil.getAppNotificationStatus(),
                is(NotificationSystemStatusUtil.APP_NOTIFICATIONS_STATUS_DISABLED));
    }
}
