// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * JUnit tests for NotificationSystemStatusUtil which run against Robolectric so that we can
 * manipulate what NotificationManagerCompat.getNotificationsEnabled returns.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationSystemStatusUtilUnitTest {
    @Test
    public void testAppNotificationStatusEnabled() {
        getShadowNotificationManager().setNotificationsEnabled(true);
        assertThat(NotificationSystemStatusUtil.getAppNotificationStatus(),
                is(NotificationSystemStatusUtil.APP_NOTIFICATIONS_STATUS_ENABLED));
    }

    @Test
    public void testAppNotificationStatusDisabled() {
        getShadowNotificationManager().setNotificationsEnabled(false);
        assertThat(NotificationSystemStatusUtil.getAppNotificationStatus(),
                is(NotificationSystemStatusUtil.APP_NOTIFICATIONS_STATUS_DISABLED));
    }

    private ShadowNotificationManager getShadowNotificationManager() {
        return shadowOf((NotificationManager) RuntimeEnvironment.application.getSystemService(
                Context.NOTIFICATION_SERVICE));
    }
}
