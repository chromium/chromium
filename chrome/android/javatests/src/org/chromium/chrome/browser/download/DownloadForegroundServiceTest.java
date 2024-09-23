// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.Service.STOP_FOREGROUND_DETACH;
import static android.app.Service.STOP_FOREGROUND_REMOVE;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Test for DownloadForegroundService. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DownloadForegroundServiceTest {
    private static final int FAKE_DOWNLOAD_ID1 = 1;
    private static final int FAKE_DOWNLOAD_ID2 = 2;

    private Notification mNotification;
    private MockDownloadForegroundService mForegroundService;

    /**
     * Implementation of DownloadForegroundService for testing. Mimics behavior of
     * DownloadForegroundService except for calls to the actual service.
     */
    public static class MockDownloadForegroundService extends DownloadForegroundServiceImpl {
        @IntDef({MethodID.START_FOREGROUND, MethodID.STOP_FOREGROUND_FLAGS})
        @Retention(RetentionPolicy.SOURCE)
        public @interface MethodID {
            int START_FOREGROUND = 0;
            int STOP_FOREGROUND_FLAGS = 1;
        }

        int mTargetSdk = 34;
        int mStopForegroundFlags = -1;
        int mNextNotificationId = INVALID_NOTIFICATION_ID;

        // Used for saving MethodID values.
        List<Integer> mMethodCalls = new ArrayList<>();

        public MockDownloadForegroundService() {
            setService(new DownloadForegroundService());
        }

        // Clears stored flags/boolean/id/method calls. Call between tests runs.
        void clearStoredState() {
            mStopForegroundFlags = -1;
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
        mNotification =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.DOWNLOADS)
                        .setSmallIcon(org.chromium.chrome.R.drawable.ic_file_download_white_24dp)
                        .setContentTitle("fakeContentTitle")
                        .setContentText("fakeContentText")
                        .build();
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

        mForegroundService.clearStoredState();

        // Test the case where there is another pinned notification and the service needs to start.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, false);
        expectedMethodCalls =
                Arrays.asList(
                        MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);

        mForegroundService.clearStoredState();

        // Test the case where there is another pinned notification but we are killing it.
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID2, mNotification, FAKE_DOWNLOAD_ID1, mNotification, true);
        expectedMethodCalls =
                Arrays.asList(
                        MockDownloadForegroundService.MethodID.STOP_FOREGROUND_FLAGS,
                        MockDownloadForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
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
                DownloadForegroundServiceImpl.StopForegroundNotification.DETACH,
                INVALID_NOTIFICATION_ID,
                null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);

        // When the service gets stopped with request to detach and kill (complete/failed).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundServiceImpl.StopForegroundNotification.DETACH,
                INVALID_NOTIFICATION_ID,
                null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_DETACH, mForegroundService.mStopForegroundFlags);

        // When the service gets stopped with request to not detach but to kill (cancel).
        mForegroundService.startOrUpdateForegroundService(
                FAKE_DOWNLOAD_ID1, mNotification, INVALID_NOTIFICATION_ID, null, false);
        mForegroundService.clearStoredState();

        mForegroundService.stopDownloadForegroundService(
                DownloadForegroundServiceImpl.StopForegroundNotification.KILL,
                INVALID_NOTIFICATION_ID,
                null);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }
}
