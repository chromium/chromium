// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Tests for NotificationManager */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mBridgeMock;
    @Mock private SendTabToSelfMetricsRecorder.Natives mMetricsMock;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        SendTabToSelfMetricsRecorderJni.setInstanceForTesting(mMetricsMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @Test
    @SmallTest
    public void testNotificationTap() {
        String guid = "test_guid";
        String url = "https://www.example.com";
        Intent intent = new Intent("send_tab_to_self.tap");
        intent.putExtra("send_tab_to_self.notification.guid", guid);
        intent.setData(android.net.Uri.parse(url));

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(any(), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        any(), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
    }
}
