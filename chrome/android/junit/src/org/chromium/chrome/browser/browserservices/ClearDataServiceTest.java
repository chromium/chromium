// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link ClearDataService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows={ ShadowPendingIntent.class })
public class ClearDataServiceTest {
    @Mock public ClearDataNotificationPublisher mNotificationManager;
    @Mock public DomainDataCleaner mCleaner;

    private ClearDataService mService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mService = new ClearDataService(mNotificationManager, mCleaner);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void dismissNotification() {
        int notificationId = 12345;

        ShadowPendingIntent intent = Shadows.shadowOf(
                ClearDataService.getDismissIntent(RuntimeEnvironment.application, notificationId));

        mService.onStartCommand(intent.getSavedIntent(), 0, 0);

        verify(mNotificationManager).dismissClearDataNotification(any(), eq(notificationId));
    }
}
