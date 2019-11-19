// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.chromium.chrome.browser.notifications.NotificationConstants.DEFAULT_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Context;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Test for DownloadForegroundServiceManager.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class DownloadForegroundServiceManagerTest {
    private static final int FAKE_DOWNLOAD_1 = 111;
    private static final int FAKE_DOWNLOAD_2 = 222;
    private static final int FAKE_DOWNLOAD_3 = 333;
    private static final String FAKE_NOTIFICATION_CHANNEL = "DownloadForegroundServiceManagerTest";

    private MockDownloadForegroundServiceManager mDownloadServiceManager;
    private Notification mNotification;
    private Context mContext;

    /**
     * Implementation of DownloadServiceManager for testing purposes.
     * Generally mimics behavior of DownloadForegroundServiceManager except:
     *  - Tracks a few variables for testing purposes (mIsServiceBound, mUpdateNotificationId, etc).
     *  - Does not actually execute code related to starting and stopping the service
     *      (startAndBindServiceInternal, etc) to not have have to handle test service lifecycle.
     */
    public static class MockDownloadForegroundServiceManager
            extends DownloadForegroundServiceManager {
        private boolean mIsServiceBound;
        private int mUpdatedNotificationId = DEFAULT_NOTIFICATION_ID;
        private int mStopForegroundNotificationFlag = -1;

        public MockDownloadForegroundServiceManager() {}

        @Override
        void startAndBindService(Context context) {
            mIsServiceBound = true;
            super.startAndBindService(context);
        }

        @Override
        void startAndBindServiceInternal(Context context) {}

        @Override
        void stopAndUnbindService(@DownloadNotificationService.DownloadStatus int downloadStatus) {
            mIsServiceBound = false;
            super.stopAndUnbindService(downloadStatus);
        }

        @Override
        void stopAndUnbindServiceInternal(@DownloadForegroundService.StopForegroundNotification
                                          int stopForegroundNotification,
                int pinnedNotificationId, Notification pinnedNotification) {
            mStopForegroundNotificationFlag = stopForegroundNotification;
        }

        @Override
        void startOrUpdateForegroundService(int notificationId, Notification notification) {
            mUpdatedNotificationId = notificationId;
            super.startOrUpdateForegroundService(notificationId, notification);
        }

        // Skip waiting for delayed runnable in tests.
        @Override
        void postMaybeStopServiceRunnable() {}

        /**
         * Call for testing that mimics the onServiceConnected call in mConnection that ensures the
         * mBoundService is non-null and the pending queue is processed.
         */
        void onServiceConnected() {
            setBoundService(new MockDownloadForegroundService());
            processDownloadUpdateQueue(true /* isProcessingPending */);
        }
    }

    /**
     * Implementation of DownloadForegroundService for testing.
     * Does not implement startOrUpdateForegroundService to avoid test service lifecycle.
     */
    public static class MockDownloadForegroundService extends DownloadForegroundService {
        @Override
        public void startOrUpdateForegroundService(int newNotificationId,
                Notification newNotification, int oldNotificationId, Notification oldNotification,
                boolean killOldNotification) {}
    }

    @Before
    public void setUp() {
        Looper.prepare();

        mContext = new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
        mDownloadServiceManager = new MockDownloadForegroundServiceManager();

        mNotification =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.ChannelId.DOWNLOADS)
                        .setSmallIcon(org.chromium.chrome.R.drawable.ic_file_download_white_24dp)
                        .setContentTitle(FAKE_NOTIFICATION_CHANNEL)
                        .setContentText(FAKE_NOTIFICATION_CHANNEL)
                        .build();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testBasicStartAndStop() {
        // Service starts and stops with addition and removal of one active download.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_1,
                mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);

        // Service does not get affected by addition of inactive download.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.PAUSED, FAKE_DOWNLOAD_2, mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);

        // Service continues as long as there is at least one active download.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_3,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.PAUSED, FAKE_DOWNLOAD_1, mNotification);
        assertEquals(FAKE_DOWNLOAD_3, mDownloadServiceManager.mUpdatedNotificationId);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_3,
                mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDelayedStartStop() {
        // Calls to start and stop service.
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_1,
                mNotification);

        assertTrue(mDownloadServiceManager.mIsServiceBound);

        // Service actually starts, should be shut down immediately.
        mDownloadServiceManager.onServiceConnected();
        assertFalse(mDownloadServiceManager.mIsServiceBound);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDelayedStartStopStart() {
        // Calls to start and stop and start service.
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_1,
                mNotification);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_2,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);

        // Service actually starts, continues and is pinned to second download.
        mDownloadServiceManager.onServiceConnected();
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        assertEquals(FAKE_DOWNLOAD_2, mDownloadServiceManager.mUpdatedNotificationId);

        // Make sure service is able to be shut down.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_2,
                mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testIsNotificationKilledOrDetached() {
        // Service starts and is paused, not complete, so notification not killed but is detached.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.PAUSED, FAKE_DOWNLOAD_1, mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        assertEquals(DownloadForegroundService.StopForegroundNotification.DETACH,
                mDownloadServiceManager.mStopForegroundNotificationFlag);

        // Service restarts and then is cancelled, so notification is killed.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_1,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.CANCELLED, FAKE_DOWNLOAD_1,
                mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        assertEquals(DownloadForegroundService.StopForegroundNotification.KILL,
                mDownloadServiceManager.mStopForegroundNotificationFlag);

        // Download starts and completes, notification is either detached or killed.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_2,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_2,
                mNotification);
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        assertEquals(DownloadForegroundService.StopForegroundNotification.DETACH,
                mDownloadServiceManager.mStopForegroundNotificationFlag);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStopInitiallyAndCleanQueue() {
        // First call is a download being cancelled.
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.CANCELLED, FAKE_DOWNLOAD_1,
                mNotification);

        // Make sure that nothing gets called, service is still not bound, and queue is empty.
        assertFalse(mDownloadServiceManager.mIsServiceBound);
        assertTrue(mDownloadServiceManager.mDownloadUpdateQueue.isEmpty());

        // Start next two downloads.
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_2,
                mNotification);
        assertEquals(1, mDownloadServiceManager.mDownloadUpdateQueue.size());
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        mDownloadServiceManager.onServiceConnected();

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_3,
                mNotification);
        assertEquals(2, mDownloadServiceManager.mDownloadUpdateQueue.size());
        assertTrue(mDownloadServiceManager.mIsServiceBound);

        // Queue is cleaned as each download becomes inactive (paused or complete).
        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.PAUSED, FAKE_DOWNLOAD_2, mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        assertEquals(1, mDownloadServiceManager.mDownloadUpdateQueue.size());

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.IN_PROGRESS, FAKE_DOWNLOAD_2,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        assertEquals(2, mDownloadServiceManager.mDownloadUpdateQueue.size());

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_2,
                mNotification);
        assertTrue(mDownloadServiceManager.mIsServiceBound);
        assertEquals(1, mDownloadServiceManager.mDownloadUpdateQueue.size());

        mDownloadServiceManager.updateDownloadStatus(mContext,
                DownloadNotificationService.DownloadStatus.COMPLETED, FAKE_DOWNLOAD_3,
                mNotification);
        assertTrue(mDownloadServiceManager.mDownloadUpdateQueue.isEmpty());
        assertFalse(mDownloadServiceManager.mIsServiceBound);
    }
}
