// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Tests for {@link DisclosureAcceptanceBroadcastReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPendingIntent.class})
public class DisclosureAcceptanceBroadcastReceiverTest {
    @Mock public NotificationManagerProxy mNotificationManager;
    @Mock public BrowserServicesStore mStore;

    private DisclosureAcceptanceBroadcastReceiver mService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mService = new DisclosureAcceptanceBroadcastReceiver(mNotificationManager, mStore);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void dismissesNotification() {
        Context context = ApplicationProvider.getApplicationContext();
        String tag = "tag";
        int id = 0;
        String packageName = "com.example";

        PendingIntentProvider provider =
                DisclosureAcceptanceBroadcastReceiver.createPendingIntent(
                        context, tag, id, packageName);

        mService.onReceive(context, extractIntent(provider));
        verify(mNotificationManager).cancel(eq(tag), eq(id));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordsAcceptance() {
        Context context = ApplicationProvider.getApplicationContext();
        String tag = "tag";
        int id = 0;
        String packageName = "com.example";

        PendingIntentProvider provider =
                DisclosureAcceptanceBroadcastReceiver.createPendingIntent(
                        context, tag, id, packageName);

        mService.onReceive(context, extractIntent(provider));
        verify(mStore).setUserAcceptedTwaDisclosureForPackage(eq(packageName));
    }

    private static Intent extractIntent(PendingIntentProvider provider) {
        ShadowPendingIntent shadow = shadowOf(provider.getPendingIntent());
        Intent intent = shadow.getSavedIntents()[0];
        assertNotNull(intent);
        return intent;
    }
}
