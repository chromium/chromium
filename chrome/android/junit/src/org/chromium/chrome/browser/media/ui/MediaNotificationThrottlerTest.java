// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * JUnit tests for checking {@link MediaNotificationController} throttles notification updates
 * correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationThrottlerTest extends MediaNotificationTestBase {
    private static final int THROTTLE_MILLIS =
            MediaNotificationController.Throttler.THROTTLE_MILLIS;
    private static final int MAX_INIT_WAIT_TIME_MILLIS =
            MediaNotificationController.PendingIntentInitializer.MAX_INIT_WAIT_TIME_MILLIS;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        getController().mThrottler =
                spy(new MediaNotificationController.Throttler(getController()));
    }

    public void simulateIdleTask() {
        getPendingIntentInitializer().createPendingIntentActionSwipeIfNeeded();
        assertNotNull(getController().mPendingIntentActionSwipe);
    }

    @Test
    public void testEmptyState() {
        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testFirstNotificationIsUnthrottled() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getThrottler().queueNotification(info);

        // Verify the notification is shown immediately
        verify(getThrottler()).showNotificationImmediately(info);
        verify(getController()).showNotification(info);

        // Verify entering the throttled state.
        assertNotNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testDelayedTaskFiresWithNoQueuedNotification() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getThrottler().queueNotification(info);
        clearInvocations(getThrottler());

        // When the task fires while no notification info is queued in throttled state, the
        // throttler enters unthrottled state.
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler(), never()).showNotificationImmediately(info);
        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testDelayedTaskFiresWithQueuedNotification() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        getThrottler().queueNotification(info1);
        getThrottler().queueNotification(info2);

        // In throttled state, queueing a notification info will only update the queued notification
        // info.
        verify(getThrottler()).showNotificationImmediately(info1);
        verify(getController()).showNotification(info1);
        verify(getThrottler()).showNotificationImmediately(info2);
        verify(getController(), never()).showNotification(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info2, getThrottler().mLastPendingInfo);
        clearInvocations(getThrottler());

        // When the delayed task fires, the queued notification info will be used to update
        // notification.
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler()).showNotificationImmediately(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testQueueingNotificationWontResetTimer() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        getThrottler().queueNotification(info1);
        // Wait till the timer goes half way.
        advanceTimeByMillis(THROTTLE_MILLIS / 2);
        getThrottler().queueNotification(info2);

        clearInvocations(getThrottler());

        // The task should fire after |THROTTLE_MILLIS| since the it is posted.
        advanceTimeByMillis(THROTTLE_MILLIS / 2);
        verify(getThrottler()).showNotificationImmediately(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);

        // Can't check whether another task is posted due to the limitation of Robolectric shadows.
    }

    @Test
    public void testQueueNotificationIgnoresNotification_Unthrottled() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo infoPlaying1 = mMediaNotificationInfoBuilder.build();
        MediaNotificationInfo infoPlaying2 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo infoPaused1 = mMediaNotificationInfoBuilder.build();
        MediaNotificationInfo infoPaused2 = mMediaNotificationInfoBuilder.build();

        // This will update the notification immediately and enter throttled state.
        getThrottler().queueNotification(infoPlaying1);

        // This will not update the queued notification as it is the same as which stored in
        // MediaNotificationController.
        getThrottler().queueNotification(infoPlaying2);
        assertNull(getThrottler().mLastPendingInfo);

        // This will update the queued notification info as it is a different one.
        getThrottler().queueNotification(infoPaused1);
        assertSame(infoPaused1, getThrottler().mLastPendingInfo);

        // This will not update the queued notification info as it is the same with the queued info.
        getThrottler().queueNotification(infoPaused2);
        assertSame(infoPaused1, getThrottler().mLastPendingInfo);

        // This will not update the queued notification info as it is different from the queued
        // info.
        getThrottler().queueNotification(infoPlaying1);
        assertSame(infoPlaying1, getThrottler().mLastPendingInfo);
    }

    @Test
    public void testQueueNotificationIgnoresNotification_Throttled() {
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        getThrottler().queueNotification(info1);
        clearInvocations(getThrottler());
        getThrottler().queueNotification(info2);

        verify(getThrottler(), never())
                .showNotificationImmediately(any(MediaNotificationInfo.class));
        assertNotNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testClearNotificationClearsThrottler() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getThrottler().queueNotification(info);

        getController().clearNotification();

        verify(getThrottler()).clearPendingNotifications();
    }

    @Test
    public void testClearThrottler() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getThrottler().queueNotification(info);
        getThrottler().clearPendingNotifications();
        clearInvocations(getThrottler());

        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);

        // Check the task is removed
        advanceTimeByMillis(THROTTLE_MILLIS);

        verify(getThrottler(), never())
                .showNotificationImmediately(any(MediaNotificationInfo.class));
    }

    @Test
    public void testClearNotificationClearsThrottlerAndPendingIntentInitializer() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info);
        getController().clearNotification();

        verify(getThrottler()).clearPendingNotifications();
        verify(getPendingIntentInitializer()).clearDelayedTask();
    }

    @Test
    public void testClearPendingIntentInitializer() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info);
        getController().clearNotification();

        clearInvocations(getThrottler());
        clearInvocations(getPendingIntentInitializer());
        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
        assertNull(getPendingIntentInitializer().mSwipeInitTask);

        // Check the task is removed
        advanceTimeByMillis(THROTTLE_MILLIS);

        verify(getThrottler(), never())
                .showNotificationImmediately(any(MediaNotificationInfo.class));
    }

    @Test
    public void testQueueNotification_PendingIntentInitializedDoesNotScheduleTasks() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();

        getController().mPendingIntentInitializer =
                spy(new MediaNotificationController.PendingIntentInitializer(getController()));
        assertNotNull(getController().mPendingIntentActionSwipe);
        getThrottler().queueNotification(info);
        verify(getPendingIntentInitializer()).schedulePendingIntentConstructionIfNeeded();
        verify(getPendingIntentInitializer(), never()).postDelayedTask();
        verify(getPendingIntentInitializer(), never()).scheduleIdleTask();
    }

    @Test
    public void testQueueNotification_DelayedInitTasksOnlyScheduledOnce() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        getController().mPendingIntentInitializer =
                spy(new MediaNotificationController.PendingIntentInitializer(getController()));
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info1);
        verify(getPendingIntentInitializer()).schedulePendingIntentConstructionIfNeeded();
        verify(getPendingIntentInitializer()).postDelayedTask();
        verify(getPendingIntentInitializer()).scheduleIdleTask();

        clearInvocations(getPendingIntentInitializer());
        getThrottler().queueNotification(info2);
        verify(getPendingIntentInitializer()).schedulePendingIntentConstructionIfNeeded();
        verify(getPendingIntentInitializer(), never()).postDelayedTask();
        verify(getPendingIntentInitializer(), never()).scheduleIdleTask();
    }

    @Test
    public void testQueueNotification_PendingIntentInitializedBeforeTrhottleMillis() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        // Simulate that the pending intent is null. Queue a notification and verify that the
        // notification is queued and we entered a throttled state.
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info1);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info1, getThrottler().mLastPendingInfo);
        verify(getThrottler()).showNotificationImmediately(info1);
        verify(getController(), never()).showNotification(info1);

        // Simulate that the pending intent has initialized. Queue a new notification and verify
        // that the new notification is queued and we continue in a throttled state.
        clearInvocations(getThrottler());
        getController().mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        getThrottler().queueNotification(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info2, getThrottler().mLastPendingInfo);
        verify(getThrottler(), never()).showNotificationImmediately(info1);
        verify(getThrottler()).showNotificationImmediately(info2);
        verify(getController(), never()).showNotification(info2);

        // When the delayed task fires, the latest queued notification info will be used to update
        // notification.
        clearInvocations(getThrottler());
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler(), never()).showNotificationImmediately(info1);
        verify(getThrottler()).showNotificationImmediately(info2);
        verify(getController()).showNotification(info2);
    }

    @Test
    public void testQueueNotification_PendingIntentInitializedAfterTrhottleMillis() {
        mMediaNotificationInfoBuilder.setPaused(false);
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.build();
        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.build();

        // Simulate that the pending intent is null. Queue a notification and verify that the
        // notification is queued and we entered a throttled state.
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info1);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info1, getThrottler().mLastPendingInfo);
        verify(getThrottler()).showNotificationImmediately(info1);
        verify(getController(), never()).showNotification(info1);

        // When the delayed task fires, the notification will not be updated, since the pending
        // intent is still null.
        clearInvocations(getThrottler());
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler()).showNotificationImmediately(info1);
        verify(getController(), never()).showNotification(info1);
        assertNotNull(getThrottler().mThrottleTask);
        assertNotNull(getThrottler().mLastPendingInfo);

        // Simulate that the pending intent has initialized. Queue a new notification and verify
        // that the new notification is queued and we continue in a throttled state.
        clearInvocations(getThrottler());
        getController().mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        getThrottler().queueNotification(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info2, getThrottler().mLastPendingInfo);
        verify(getThrottler(), never()).showNotificationImmediately(info1);
        verify(getThrottler()).showNotificationImmediately(info2);
        verify(getController(), never()).showNotification(info2);

        // When the delayed task fires, the latest queued notification info will be used to update
        // notification.
        clearInvocations(getThrottler());
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler(), never()).showNotificationImmediately(info1);
        verify(getThrottler()).showNotificationImmediately(info2);
        verify(getController()).showNotification(info2);
        assertNotNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    @Test
    public void testQueueNotification_PendingIntentInitByIdleTask() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();

        getController().mPendingIntentInitializer =
                spy(new MediaNotificationController.PendingIntentInitializer(getController()));
        doNothing().when(getPendingIntentInitializer()).postDelayedTask();

        // Simulate that the pending intent is null, queue a notification and wait for the idle task
        // to complete.
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info);
        assertNull(getPendingIntentInitializer().mSwipeInitTask);
        simulateIdleTask();
        advanceTimeByMillis(MAX_INIT_WAIT_TIME_MILLIS);

        // Verify that: the notification is shown, throttled state ended and pending intent tasks
        // are cleared.
        verify(getThrottler(), times(2)).showNotificationImmediately(info);
        verify(getController()).showNotification(info);
        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
        assertNotNull(getController().mPendingIntentActionSwipe);
        assertNull(getPendingIntentInitializer());
    }

    @Test
    public void testQueueNotification_PendingIntentInitByDelayedTask() {
        MediaNotificationInfo info = mMediaNotificationInfoBuilder.build();

        getController().mPendingIntentInitializer =
                spy(new MediaNotificationController.PendingIntentInitializer(getController()));

        // Do "nothing" when `scheduleIdleTask` is called, since we want to force the delayed task
        // to initialize the pending intent.
        doNothing().when(getPendingIntentInitializer()).scheduleIdleTask();

        // Simulate that the pending intent is null. Queue a notification and verify that the
        // notification is queued and we entered a throttled state.
        getController().mPendingIntentActionSwipe = null;
        getThrottler().queueNotification(info);
        assertNotNull(getThrottler().mThrottleTask);
        assertSame(info, getThrottler().mLastPendingInfo);
        verify(getThrottler()).showNotificationImmediately(info);
        verify(getController(), never()).showNotification(info);
        verify(getPendingIntentInitializer()).schedulePendingIntentConstructionIfNeeded();
        assertNotNull(getPendingIntentInitializer().mSwipeInitTask);
        assertNull(getController().mPendingIntentActionSwipe);

        // When the delayed task fires, the notification will not be updated, since the pending
        // intent is still null.
        clearInvocations(getThrottler());
        clearInvocations(getPendingIntentInitializer());
        advanceTimeByMillis(THROTTLE_MILLIS);
        verify(getThrottler()).showNotificationImmediately(info);
        verify(getController(), never()).showNotification(info);
        assertNotNull(getThrottler().mThrottleTask);
        assertNotNull(getThrottler().mLastPendingInfo);
        assertNull(getController().mPendingIntentActionSwipe);

        // Wait for the delayed task to initialize the pending intent, and verify that the
        // notification is updated.
        clearInvocations(getThrottler());
        clearInvocations(getPendingIntentInitializer());
        advanceTimeByMillis(MAX_INIT_WAIT_TIME_MILLIS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertNull(getPendingIntentInitializer());
        assertNotNull(getController().mPendingIntentActionSwipe);
        verify(getThrottler(), times(3)).showNotificationImmediately(info);
        verify(getController()).showNotification(info);
        assertNull(getThrottler().mThrottleTask);
        assertNull(getThrottler().mLastPendingInfo);
    }

    private MediaNotificationController.Throttler getThrottler() {
        return getController().mThrottler;
    }

    private MediaNotificationController.PendingIntentInitializer getPendingIntentInitializer() {
        return getController().mPendingIntentInitializer;
    }
}
