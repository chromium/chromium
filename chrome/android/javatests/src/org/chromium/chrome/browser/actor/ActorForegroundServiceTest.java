// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;

import android.app.Notification;

import androidx.annotation.IntDef;
import androidx.core.app.ServiceCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Test for ActorForegroundService. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
public class ActorForegroundServiceTest {
    private static final int NOTIFICATION_ID_1 = 1;
    private static final int NOTIFICATION_ID_2 = 2;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Notification mNotification;
    private MockActorForegroundService mForegroundService;

    /**
     * Implementation of ActorForegroundService for testing. Mimics behavior of
     * ActorForegroundService except for calls to the actual service.
     */
    public static class MockActorForegroundService extends ActorForegroundServiceImpl {
        @IntDef({MethodID.START_FOREGROUND, MethodID.STOP_FOREGROUND})
        @Retention(RetentionPolicy.SOURCE)
        public @interface MethodID {
            int START_FOREGROUND = 0;
            int STOP_FOREGROUND = 1;
        }

        int mStopForegroundFlags = -1;

        // Used for saving MethodID values.
        List<Integer> mMethodCalls = new ArrayList<>();

        public MockActorForegroundService() {
            setService(new ActorForegroundService());
        }

        // Clears stored flags/boolean/id/method calls. Call between tests runs.
        void clearStoredState() {
            mStopForegroundFlags = -1;
            mMethodCalls.clear();
        }

        @Override
        void startForegroundInternal(int notificationId, Notification notification) {
            mMethodCalls.add(MethodID.START_FOREGROUND);
        }

        @Override
        void stopForegroundInternal(int flags) {
            mMethodCalls.add(MethodID.STOP_FOREGROUND);
            mStopForegroundFlags = flags;
        }
    }

    @Before
    public void setUp() {
        mForegroundService = new MockActorForegroundService();
        mNotification =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.DOWNLOADS)
                        .setSmallIcon(R.drawable.ic_file_download_white_24dp)
                        .setContentTitle("fakeContentTitle")
                        .setContentText("fakeContentText")
                        .build();
    }

    @Test
    @SmallTest
    @Feature({"Actor"})
    public void testStartOrUpdateForegroundService() {
        List<Integer> expectedMethodCalls;

        // Test starting the foreground service with an initial notification.
        mForegroundService.startOrUpdateForegroundService(NOTIFICATION_ID_1, mNotification);
        expectedMethodCalls = Arrays.asList(MockActorForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);

        mForegroundService.clearStoredState();

        // Test updating the foreground service with a different notification ID.
        mForegroundService.startOrUpdateForegroundService(NOTIFICATION_ID_2, mNotification);
        expectedMethodCalls = Arrays.asList(MockActorForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
        assertEquals(-1, mForegroundService.mStopForegroundFlags);

        mForegroundService.clearStoredState();

        // Test updating the foreground service when the notification ID is unchanged.
        mForegroundService.startOrUpdateForegroundService(NOTIFICATION_ID_2, mNotification);
        expectedMethodCalls = Arrays.asList(MockActorForegroundService.MethodID.START_FOREGROUND);
        assertEquals(expectedMethodCalls, mForegroundService.mMethodCalls);
    }

    @Test
    @SmallTest
    @Feature({"Actor"})
    public void testStopActorForegroundService() {
        mForegroundService.stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);

        assertEquals(
                Arrays.asList(MockActorForegroundService.MethodID.STOP_FOREGROUND),
                mForegroundService.mMethodCalls);
        assertEquals(ServiceCompat.STOP_FOREGROUND_REMOVE, mForegroundService.mStopForegroundFlags);
    }
}
