// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.NotificationManager;
import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;


import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link ClearDataNotificationPublisher}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows={ ShadowNotificationManager.class })
public class ClearDataNotificationPublisherTest {
    private ClearDataNotificationPublisher mClearDataNotificationPublisher;
    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    @Before
    public void setUp() {
        mClearDataNotificationPublisher = new ClearDataNotificationPublisher();
        mContext = RuntimeEnvironment.application;
        mShadowNotificationManager = Shadows.shadowOf((NotificationManager.from(mContext)));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showNotification() {
        mClearDataNotificationPublisher.showClearDataNotification(
                mContext, "App", "https://www.example.com/", true);
        Assert.assertEquals(1, mShadowNotificationManager.getAllNotifications().size());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void singleNotificationPerOrigin() {
        mClearDataNotificationPublisher.showClearDataNotification(
                mContext, "App", "https://www.website.com/", true);
        mClearDataNotificationPublisher.showClearDataNotification(
                mContext, "App 2", "https://www.website.com/", true);
        Assert.assertEquals(1, mShadowNotificationManager.getAllNotifications().size());

        mClearDataNotificationPublisher.showClearDataNotification(
                mContext, "App", "https://www.otherwebsite.com/", true);
        Assert.assertEquals(2, mShadowNotificationManager.getAllNotifications().size());
    }
}
