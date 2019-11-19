// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.Service.STOP_FOREGROUND_DETACH;
import static android.app.Service.STOP_FOREGROUND_REMOVE;

import static junit.framework.Assert.assertEquals;

import static org.chromium.chrome.browser.download.DownloadForegroundService.clearPersistedNotificationId;
import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.support.test.filters.SmallTest;
import android.support.v4.app.ServiceCompat;

import androidx.annotation.IntDef;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Test for DownloadForegroundService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadForegroundServiceTest {
    private static final int FAKE_DOWNLOAD_ID1 = 1;
    private static final int FAKE_DOWNLOAD_ID2 = 2;

    private Notification mNotification;
    private MockDownloadForegroundService mForegroundService;

    /**
     * Implementation of DownloadForegroundService for testing.
     * Mimics behavior of DownloadForegroundService except for calls to the actual service.
     */
    public static class MockDownloadForegroundService extends DownloadForegroundService {
        @IntDef({MethodID.START_FOREGROUND, MethodID.STOP_FOREGROUND_FLAGS,
                MethodID.RELAUNCH_NOTIFICATION})
        @Retention(RetentionPolicy.SOURCE)
        public @interface MethodID {
            int START_FOREGROUND = 0;
            int STOP_FOREGROUND_FLAGS = 1;
            int RELAUNCH_NOTIFICATION = 2;
        }

        int mTargetSdk = 20;
        int mStopForegroundFlags = -1;
        int mRelaunchedNotificationId = INVALID_NOTIFICATION_ID;
        int mNextNotificationId = INVALID_NOTIFICATION_ID;

        // Used for saving MethodID values.
        List<Integer> mMethodCalls = new ArrayList<>();

        // Clears stored flags/boolean/id/method calls. Call between tests runs.
        void clearStoredState() {
            mStopForegroundFlags = -1;
            mRelaunchedNotificationId = INVALID_NOTIFICATION_ID;
            mMethodCalls.clear();
            mNextNotificationId = INVALID_NOTIFICATION_ID;
        }

        @Override
        void startForegroundInternal(int notificationId, Notification notification) {
            mMethodCalls.add(MethodID.START_FOREGROUND);
        }

        @Override
        void stopForegroundInternal(int flags) {
            mMethodCalls.add(MethodID.STOP_FOREGROUND_FLAGS);
            mStopForegroundFlags = flags;
        }

        @Override
        void relaunchOldNotification(int notificationId, Notification notification) {
            mMethodCalls.add(MethodID.RELAUNCH_NOTIFICATION);
            mRelaunchedNotificationId = notificationId;
        }

        @Override
        int getCurrentSdk() {
            return mTargetSdk;
        }

        @Override
        int getNewNotificationIdFor(int oldNotificationId) {
            return mNextNotificationId;
        }
    }

    @Before
    public void setUp() {
        mForegroundService = new MockDownloadForegroundService();
        clearPersistedNotificationId();
        mNotification =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.ChannelId.DOWNLOADS)
                        .setSmallIcon(org.chromium.chrome.R.drawable.ic_file_download_white_24dp)
                        .setContentTitle("fakeContentTitle")
                        .setContentText("fakeContentText")
                        .build();
    }

    @After
    public void tearDown() {
        clearPersistedNotificationId();
    }

    /**
     * The expected behavior for start foreground when the API >= 24 is that the old notification is
     * able to be detached and the new notification pinned without any need for relaunching or
     * correcting the notification.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStartForeground_sdkAtLeast24() {
        mForegroundService.mTargetSdk = 24;
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.START_FOREGROUND);

        // Test the case where there is no other pinned notification and the service starts.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(INVALID_NOTIFICATION_ID, mForegroundService.mRelaunchedNotificationId);

        mForegroundService.clearStoredState();

        // Test the case where there is another pinned notification and the service needs to start.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, false);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);
        assertEquals(INVALID_NOTIFICATION_ID, mForegroundService.mRelaunchedNotificationId);

        mForegroundService.clearStoredState();

        // Test the case where there is another pinned notification but we are killing it.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, true);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
        assertEquals(INVALID_NOTIFICATION_ID, mForegroundService.mRelaunchedNotificationId);
    }

    /**
     * The expected behavior for start foreground when API < 24 is that the foreground is stopped
     * and, in cases there is a previously pinned notification, it is relaunched.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStartForeground_sdkLessThan24() {
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.START_FOREGROUND);

        // Test the case where there is no other pinned notification and the service starts.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(INVALID_NOTIFICATION_ID, mForegroundService.mRelaunchedNotificationId);

        mForegroundService.clearStoredState();

        // Test the case where there is another pinned notification and the service needs to start.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, false);
        expectedMethodCalls = Arrays.asList(MockDownloadForegroundService.MethodID.START_FOREGROUND,
                MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(FAKE_DOWNLOAD_ID1, mForegroundService.mRelaunchedNotificationId);

        mForegroundService.clearStoredState();

        /// Test the case where there is another pinned notification but we are killing it.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, true);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(INVALID_NOTIFICATION_ID, mForegroundService.mRelaunchedNotificationId);
    }

    /**
     * The expected behavior for stop foreground when API >= 24 is that only one call is needed,
     * stop foreground with the correct flag and no notification adjustment is required.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStopForeground_sdkAtLeast24() {
        mForegroundService.mTargetSdk = 24;
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);

        // When the service gets stopped with request to detach but not kill notification (pause).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH,
                INVALID_NOTIFICATION_ID, null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);

        // When the service gets stopped with request to detach and kill (complete/failed).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH,
                INVALID_NOTIFICATION_ID, null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);

        // When the service gets stopped with request to not detach but to kill (cancel).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.KILL, INVALID_NOTIFICATION_ID,
                null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }

    /**
     * The expected behavior for stop foreground when 24 > API >= 23 is:
     *  - paused: the notification does not get killed and is not handled properly so is persisted.
     *  - complete/failed: the notification gets killed but relaunched.
     *  - cancel: the notification gets killed and not relaunched.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStopForeground_sdkAtLeast23() {
        mForegroundService.mTargetSdk = 23;

        // When the service gets stopped with request to detach but not kill notification (pause).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);

        mForegroundService.clearStoredState();

        // When the service gets stopped with request to detach and kill (complete/failed).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
        assertEquals(FAKE_DOWNLOAD_ID1, mForegroundService.mRelaunchedNotificationId);

        // When the service gets stopped with request to not detach but to kill (cancel).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.KILL, FAKE_DOWNLOAD_ID1,
                mNotification);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }

    /**
     * The expected behavior for stop foreground when 23 > API >= 21 is similar to the previous case
     * except that in the case where a relaunch is needed (complete/failed), the relaunch needs to
     * happen before the service is stopped and requires a "new" notification id.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStopForeground_sdkAtLeast21() {
        mForegroundService.mTargetSdk = 21;

        // When the service gets stopped with request to detach but not kill notification (pause).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION,
                        MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);

        // When the service gets stopped with request to detach and kill (complete/failed).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.mNextNotificationId = FAKE_DOWNLOAD_ID2;
        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION,
                        MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
        assertEquals(mForegroundService.mNextNotificationId,
                mForegroundService.mRelaunchedNotificationId);

        // When the service gets stopped with request to not detach but to kill (cancel).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.KILL, FAKE_DOWNLOAD_ID1,
                mNotification);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }

    /**
     * The expected behavior of stop foreground when API < 21 is that the notification is killed in
     * all cases and relaunched in the pause and complete/failed case. When the notification is
     * relaunched, it is done so before the foreground is stopped and has a new notification id.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testStopForeground_sdkAtLessThan21() {
        // When the service gets stopped with request to detach but not kill notification (pause).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.mNextNotificationId = FAKE_DOWNLOAD_ID2;
        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        List<Integer> expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.RELAUNCH_NOTIFICATION,
                        MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
        assertEquals(mForegroundService.mNextNotificationId,
                mForegroundService.mRelaunchedNotificationId);

        // When the service gets stopped with request to detach and kill (complete/failed).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.mNextNotificationId = FAKE_DOWNLOAD_ID2;
        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.DETACH, FAKE_DOWNLOAD_ID1,
                mNotification);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
        assertEquals(mForegroundService.mNextNotificationId,
                mForegroundService.mRelaunchedNotificationId);

        // When the service gets stopped with request to not detach but to kill (cancel).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundService.StopForegroundNotification.KILL, FAKE_DOWNLOAD_ID1,
                mNotification);
        expectedMethodCalls =
                Arrays.asList(MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }
}
