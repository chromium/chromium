// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test for {@link DownloadUserInitiatedTaskManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class DownloadUserInitiatedTaskManagerTest {
    private static final int FAKE_DOWNLOAD_1 = 111;
    private static final int FAKE_DOWNLOAD_2 = 222;
    private static final String FAKE_NOTIFICATION_CHANNEL = "DownloadUserInitiatedTaskManagerTest";

    private MockDownloadUserInitiatedTaskManager mDownloadUITaskManager;
    private Notification mNotification;
    private Context mContext;

    public static class MockDownloadUserInitiatedTaskManager
            extends DownloadUserInitiatedTaskManager {
        public MockDownloadUserInitiatedTaskManager() {}

        public void assertPinnedNotificationId(int notificationId) {
            Assert.assertEquals(notificationId, mPinnedNotificationId);
        }
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContext = new AdvancedMockContext(ApplicationProvider.getApplicationContext());
            mDownloadUITaskManager = new MockDownloadUserInitiatedTaskManager();

            mNotification =
                    NotificationWrapperBuilderFactory
                            .createNotificationWrapperBuilder(
                                    ChromeChannelDefinitions.ChannelId.DOWNLOADS)
                            .setSmallIcon(
                                    org.chromium.chrome.R.drawable.ic_file_download_white_24dp)
                            .setContentTitle(FAKE_NOTIFICATION_CHANNEL)
                            .setContentText(FAKE_NOTIFICATION_CHANNEL)
                            .build();
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testBasicStartAndCompleteDownload() {
        // Start a download and complete.
        mDownloadUITaskManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        // TODO(crbug/1415186): Assert setNotification called.

        mDownloadUITaskManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        // TODO(crbug/1415186): Assert setNotification called.

        // Service does not get affected by addition of inactive download.
        mDownloadUITaskManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.FAILED, FAKE_DOWNLOAD_2, mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
    }
}
