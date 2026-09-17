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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
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

    private final TestTaskFinishedCallback mCallback1 = new TestTaskFinishedCallback();
    private final TestTaskFinishedCallback mCallback2 = new TestTaskFinishedCallback();

    private MockDownloadUserInitiatedTaskManager mDownloadUiTaskManager;
    private Notification mNotification;
    private Context mContext;

    private static class TestTaskFinishedCallback implements TaskFinishedCallback {
        private int mLastNotificationId = -1;
        private Notification mLastNotification;
        private int mCallCount;

        @Override
        public void taskFinished(boolean needsReschedule) {}

        @Override
        public void setNotification(int notificationId, Notification notification) {
            mLastNotificationId = notificationId;
            mLastNotification = notification;
            mCallCount++;
        }

        public void reset() {
            mLastNotificationId = -1;
            mLastNotification = null;
            mCallCount = 0;
        }
    }

    private static class MockDownloadUserInitiatedTaskManager
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
        mCallback1.reset();
        mCallback2.reset();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContext = new AdvancedMockContext(ApplicationProvider.getApplicationContext());
                    mDownloadUiTaskManager = new MockDownloadUserInitiatedTaskManager();

                    mNotification =
                            NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                            ChromeChannelDefinitions.ChannelId.DOWNLOADS)
                                    .setSmallIcon(R.drawable.ic_file_download_white_24dp)
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
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback1);

        // Start a download.
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.FAILED,
                FAKE_DOWNLOAD_1,
                mNotification);
        Assert.assertEquals(0, mCallback1.mCallCount);
        mDownloadUiTaskManager.assertPinnedNotificationId(-1);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testAddMultipleCallbacks() {
        // Set task callbacks.
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback1);
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_2, mCallback2);

        // Start a download.
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(1, mCallback1.mCallCount);
        Assert.assertEquals(FAKE_DOWNLOAD_1, mCallback1.mLastNotificationId);
        Assert.assertEquals(mNotification, mCallback1.mLastNotification);
        Assert.assertEquals(1, mCallback2.mCallCount);
        Assert.assertEquals(FAKE_DOWNLOAD_1, mCallback2.mLastNotificationId);
        Assert.assertEquals(mNotification, mCallback2.mLastNotification);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testSetNotificationAfterFinishTask() {
        // Set task callbacks.
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback1);

        // Finish the job. Equivalently the callback should be set to null.
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, null);

        // Start a download.
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        Assert.assertEquals(0, mCallback1.mCallCount);
        mDownloadUiTaskManager.assertPinnedNotificationId(-1);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testStartDownloadAndCompleteAlongWithInactiveOtherDownloads() {
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback1);
        // Start a download and complete.
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(1, mCallback1.mCallCount);
        Assert.assertEquals(FAKE_DOWNLOAD_1, mCallback1.mLastNotificationId);
        Assert.assertEquals(mNotification, mCallback1.mLastNotification);

        mCallback1.reset();
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(0, mCallback1.mCallCount);

        // Service does not get affected by addition of inactive download.
        mCallback1.reset();
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.CANCELLED,
                FAKE_DOWNLOAD_2,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(0, mCallback1.mCallCount);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testDownloadResumeAfterNetworkInterruption() {
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback1);
        // Start a download.
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(1, mCallback1.mCallCount);
        Assert.assertEquals(FAKE_DOWNLOAD_1, mCallback1.mLastNotificationId);
        Assert.assertEquals(mNotification, mCallback1.mLastNotification);

        // Start another job (due to network interruption).
        mDownloadUiTaskManager.setTaskNotificationCallback(TASK_ID_1, mCallback2);
        mDownloadUiTaskManager.updateDownloadStatus(
                mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS,
                FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadUiTaskManager.assertPinnedNotificationId(FAKE_DOWNLOAD_1);
        Assert.assertEquals(1, mCallback2.mCallCount);
        Assert.assertEquals(FAKE_DOWNLOAD_1, mCallback2.mLastNotificationId);
        Assert.assertEquals(mNotification, mCallback2.mLastNotification);
    }
}
