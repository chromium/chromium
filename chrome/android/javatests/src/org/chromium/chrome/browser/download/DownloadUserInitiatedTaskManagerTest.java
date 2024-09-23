// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.content.Context;

import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;

/** Test for {@link DownloadUserInitiatedTaskManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class DownloadUserInitiatedTaskManagerTest {
    private static final int FAKE_DOWNLOAD_1 = 111;
    private static final int FAKE_DOWNLOAD_2 = 222;

    private static final int TASK_ID_1 = 55;
    private static final int TASK_ID_2 = 56;

    private static final String FAKE_NOTIFICATION_CHANNEL = "DownloadUserInitiatedTaskManagerTest";

    private MockDownloadUserInitiatedTaskManager mDownloadUITaskManager;
    private Notification mNotification;
    private Context mContext;

    @Mock private TaskFinishedCallback mCallback1;
    @Mock private TaskFinishedCallback mCallback2;

    TaskFinishedCallback mCallback3 = Mockito.mock(TaskFinishedCallback.class);
    TaskFinishedCallback mCallback4 = Mockito.mock(TaskFinishedCallback.class);

    public static class MockDownloadUserInitiatedTaskManager
            extends DownloadUserInitiatedTaskManager {
        public MockDownloadUserInitiatedTaskManager() {}

        @Override
        boolean isEnabled() {
            return true;
        }

        public void assertPinnedNotificationId(int notificationId) {
            Assert.assertEquals(notificationId, mPinnedNotificationId);
        }
    }

    @Before
    public void setUp() {
        // MockitoAnnotations.initMocks(this);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContext = new AdvancedMockContext(ApplicationProvider.getApplicationContext());
                    mDownloadUITaskManager = new MockDownloadUserInitiatedTaskManager();

                    mNotification =
                            NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                            ChromeChannelDefinitions.ChannelId.DOWNLOADS)
                                    .setSmallIcon(
                                            org.chromium.chrome.R.drawable
                                                    .ic_file_download_white_24dp)
                                    .setContentTitle(FAKE_NOTIFICATION_CHANNEL)
                                    .setContentText(FAKE_NOTIFICATION_CHANNEL)
                                    .build();
                });
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testCallbackNotInvokedForNonInProgressStates() {
        // Set task callbacks.
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback3);

        // Start a download.
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.FAILED,
                FAKE_DOWNLOAD_1,
                mNotification);
        Mockito.verify(mCallback3, Mockito.times(0))
                .setNotification(FAKE_DOWNLOAD_1, mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(-1);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testAddMultipleCallbacks() {
        // Set task callbacks.
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback3);
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_2, mCallback4);

        // Start a download.
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback3).setNotification(FAKE_DOWNLOAD_1, mNotification);
        Mockito.verify(mCallback4).setNotification(FAKE_DOWNLOAD_1, mNotification);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSetNotificationAfterFinishTask() {
        // Set task callbacks.
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback3);

        // Finish the job. Equivalently the callback should be set to null.
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, null);

        // Start a download.
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        Mockito.verify(mCallback3, Mockito.times(0))
                .setNotification(FAKE_DOWNLOAD_1, mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(-1);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testStartDownloadAndCompleteAlongWithInactiveOtherDownloads() {
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback3);
        // Start a download and complete.
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback3).setNotification(FAKE_DOWNLOAD_1, mNotification);

        Mockito.clearInvocations(mCallback3);
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback3, Mockito.times(0))
                .setNotification(FAKE_DOWNLOAD_1, mNotification);

        // Service does not get affected by addition of inactive download.
        Mockito.clearInvocations(mCallback3);
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.CANCELLED,
                FAKE_DOWNLOAD_2,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback3, Mockito.times(0))
                .setNotification(FAKE_DOWNLOAD_1, mNotification);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testDownloadResumeAfterNetworkInterruption() {
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback3);
        // Start a download.
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback3).setNotification(FAKE_DOWNLOAD_1, mNotification);

        // Start another job (due to network interruption).
        mDownloadUITaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback4);
        mDownloadUITaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUITaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Mockito.verify(mCallback4).setNotification(FAKE_DOWNLOAD_1, mNotification);
    }
}
