// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.ShadowDeviceConditions;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflineNotificationBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class, ShadowDeviceConditions.class})
public class OfflineNotificationBackgroundTaskUnitTest {
    /**
     * Fake of BackgroundTaskScheduler system service.
     */
    public static class FakeBackgroundTaskScheduler implements BackgroundTaskScheduler {
        private HashMap<Integer, TaskInfo> mTaskInfos = new HashMap<>();

        @Override
        public boolean schedule(Context context, TaskInfo taskInfo) {
            mTaskInfos.put(taskInfo.getTaskId(), taskInfo);
            return true;
        }

        @Override
        public void cancel(Context context, int taskId) {
            mTaskInfos.remove(taskId);
        }

        @Override
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}

        public TaskInfo getTaskInfo(int taskId) {
            return mTaskInfos.get(taskId);
        }

        public void clear() {
            mTaskInfos = new HashMap<>();
        }
    }

    @Spy
    private OfflineNotificationBackgroundTask mOfflineNotificationBackgroundTask =
            new OfflineNotificationBackgroundTask();
    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Captor
    private ArgumentCaptor<BrowserParts> mBrowserParts;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private PrefetchedPagesNotifier mPrefetchedPagesNotifier;

    private FakeBackgroundTaskScheduler mFakeTaskScheduler;
    private Calendar mCalendar;

    private String mContentHost = "www.example.com";

    @SuppressWarnings("unchecked")
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Set up the context.
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mBrowserParts.getValue().finishNativeInitialization();
                return null;
            }
        })
                .when(mChromeBrowserInitializer)
                .handlePostNativeStartup(eq(true), mBrowserParts.capture());

        doAnswer((invocation) -> {
            Object callback = invocation.getArguments()[1];
            ((Callback<String>) callback).onResult(mContentHost);
            return null;
        })
                .when(mOfflinePageBridge)
                .checkForNewOfflineContent(anyLong(), any(Callback.class));
        OfflineNotificationBackgroundTask.setOfflinePageBridgeForTesting(mOfflinePageBridge);

        doNothing().when(mPrefetchedPagesNotifier).recordNotificationAction(anyInt());
        PrefetchedPagesNotifier.setInstanceForTest(mPrefetchedPagesNotifier);

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        mFakeTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeTaskScheduler);

        mCalendar = Calendar.getInstance();
        // Emulate it being January 1, 2017 00:00:00 at the start of each test.
        mCalendar.clear();
        mCalendar.set(2017, 1, 1, 0, 0, 0);

        OfflineNotificationBackgroundTask.setCalendarForTesting(mCalendar);
        PrefetchPrefs.setNotificationEnabled(true);
    }

    @After
    public void tearDown() {
        // Ensure that an empty content notificaition is not shown in any test.
        verify(mPrefetchedPagesNotifier, never()).showNotification("");
    }

    /**
     * Runs mOfflineNotificationBackgroundTask with the given params.
     * Asserts that reschedule was called exactly once and returns the reschedule value.
     */
    private void runTask() {
        TaskParameters params = mock(TaskParameters.class);
        when(params.getTaskId()).thenReturn(TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID);

        final ArrayList<Boolean> reschedules = new ArrayList<>();
        mOfflineNotificationBackgroundTask.onStartTask(RuntimeEnvironment.application, params,
                (reschedule) -> reschedules.add(reschedule));

        assertEquals("When running the task, the TaskCompletionCallback should not have run.", 0,
                reschedules.size());
    }

    private void runTaskAndExpectTaskDone() {
        TaskParameters params = mock(TaskParameters.class);
        when(params.getTaskId()).thenReturn(TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID);

        final ArrayList<Boolean> reschedules = new ArrayList<>();
        mOfflineNotificationBackgroundTask.onStartTask(RuntimeEnvironment.application, params,
                (reschedule) -> reschedules.add(reschedule));

        assertFalse("When running the task, the TaskCompletionCallback should run.",
                reschedules.get(0));
    }

    private void setupDeviceOnlineStatus(boolean online) {
        DeviceConditions deviceConditions = new DeviceConditions(false /* POWER_CONNECTED */,
                75 /* BATTERY_LEVEL */,
                online ? ConnectionType.CONNECTION_WIFI : ConnectionType.CONNECTION_NONE,
                false /* POWER_SAVE */, false /* metered */, true /* screenOnAndUnlocked */);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);
    }

    public void assertTaskScheduledForOfflineDelay(String message) {
        assertTaskScheduledForDelay(
                OfflineNotificationBackgroundTask.OFFLINE_POLL_DELAY_MINUTES, message);
    }

    public void assertTaskScheduledForOnlineDelay(String message) {
        assertTaskScheduledForDelay(
                OfflineNotificationBackgroundTask.DEFAULT_START_DELAY_MINUTES, message);
    }

    public void assertTaskScheduledForTomorrowMorning(String message) {
        // 31 hours = 24 hours + 7 (7am on January 2, 2017).
        assertTaskScheduledForDelay(31 * 60, message);
    }

    public void assertTaskScheduledForDelay(long delay, String message) {
        TaskInfo task =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID);
        assertNotNull("While asserting that: " + message + ", the task was not scheduled.", task);
        long delayInMillis = TimeUnit.MINUTES.toMillis(delay);
        assertEquals(message, delayInMillis, task.getOneOffInfo().getWindowStartTimeMs());
        assertEquals("While asserting that: " + message + ", the task was not persisted.", true,
                task.isPersisted());
    }

    public void assertNoTaskScheduled(String message) {
        TaskInfo task =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID);
        assertNull(message, task);
    }

    private void assertNativeDidNotStart() {
        verify(mChromeBrowserInitializer, never()).handlePreNativeStartup(any(BrowserParts.class));
    }

    private void assertNativeStarted() {
        verify(mChromeBrowserInitializer, atLeastOnce())
                .handlePreNativeStartup(any(BrowserParts.class));
    }

    private void assertNotificationNotShown() {
        verify(mPrefetchedPagesNotifier, never()).showNotification(anyString());
    }

    private void assertNotificationShown() {
        verify(mPrefetchedPagesNotifier, atLeastOnce()).showNotification("www.example.com");
        assertFalse("The notification should reset the new pages flag.",
                PrefetchPrefs.getHasNewPages());
    }

    @Test
    public void scheduleTaskAsIfOnline() {
        PrefetchPrefs.setHasNewPages(true);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertTaskScheduledForOnlineDelay("The scheduled task should have the online delay.");
    }

    @Test
    public void scheduleTaskAsIfOffline() {
        PrefetchPrefs.setHasNewPages(true);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_OFFLINE);
        assertTaskScheduledForOfflineDelay("The scheduled task should have the offline delay.");
    }

    @Test
    public void startTaskWithNoNewPages() {
        PrefetchPrefs.setHasNewPages(false);
        setupDeviceOnlineStatus(false);

        runTask();

        assertNoTaskScheduled("When no new pages exist, no task should be scheduled.");
        assertNativeDidNotStart();
    }

    @Test
    public void startTaskWhileOnline() {
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(true);

        runTask();

        assertTaskScheduledForOnlineDelay("When online, the task should have the online delay.");
        assertNativeDidNotStart();
    }

    @Test
    public void startTaskWhileOffline() {
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(false);

        runTask();

        assertTaskScheduledForOfflineDelay("When offline, the task should have the offline delay.");
        assertNativeDidNotStart();
    }

    @Test
    public void offlineCounterSchedulesNotification() {
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(false);

        // Run the task almost enough times.
        for (int i = 0; i < OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS - 1; i++) {
            runTask();
            assertNativeDidNotStart();
        }
        runTaskAndExpectTaskDone();
        assertNativeStarted();
        assertNotificationShown();
    }

    @Test
    public void onlineResetsOfflineCounter() {
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(false);

        // Run the task almost enough times.
        for (int i = 0; i < OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS - 1; i++) {
            runTask();
            assertNativeDidNotStart();
        }
        setupDeviceOnlineStatus(true);
        // Run once while online.
        runTask();
        assertNativeDidNotStart();

        setupDeviceOnlineStatus(false);
        // Run the task almost enough times.
        for (int i = 0; i < OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS - 1; i++) {
            runTask();
            assertNativeDidNotStart();
        }

        // Then run it the final time and expect a notification.
        runTaskAndExpectTaskDone();
        assertNativeStarted();
        assertNotificationShown();
    }

    @Test
    public void showingNotificationCausesADelayUntilTomorrow() {
        // Setup task as though online
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(true);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertTaskScheduledForOnlineDelay("Task should be scheduled for online mode.");

        // Then simulate offline-ness and cause notification.
        setupDeviceOnlineStatus(false);
        for (int i = 0; i < OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS - 1; i++) {
            runTask();
        }
        runTaskAndExpectTaskDone();

        // Then restart again, see if the next task is scheduled.
        setupDeviceOnlineStatus(true);
        PrefetchPrefs.setHasNewPages(true);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertTaskScheduledForTomorrowMorning(
                "When we see new content after a notification, we should "
                + "reschedule for the next morning.");

        // Fast forward past 7am tomorrow morning.
        mCalendar.add(Calendar.DATE, 1);
        mCalendar.set(Calendar.HOUR_OF_DAY, 8);

        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertTaskScheduledForOnlineDelay(
                "After the next morning passes, the normal delay should occur.");
    }

    @Test
    public void ignoredNotificationPreventsSchedulingTask() {
        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(true);
        PrefetchPrefs.setIgnoredNotificationCounter(
                OfflineNotificationBackgroundTask.IGNORED_NOTIFICATION_MAX);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertNoTaskScheduled("If the notifications were ignored, the task should not be "
                + "scheduled while online.");
        setupDeviceOnlineStatus(false);
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_OFFLINE);
        assertNoTaskScheduled("If the notifications were ignored, the task should not be "
                + "scheduled while offline..");
    }

    @Test
    public void ignoredNotificationPreventsNotificationShow() {
        // Set up the prefs so that a notification would be shown, if not for the ignored
        // notification counter.
        PrefetchPrefs.setHasNewPages(true);
        PrefetchPrefs.setOfflineCounter(OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS);
        setupDeviceOnlineStatus(false);

        // Set up the ignored notification counter.
        PrefetchPrefs.setIgnoredNotificationCounter(
                OfflineNotificationBackgroundTask.IGNORED_NOTIFICATION_MAX);
        runTask();
        assertNativeDidNotStart();
        assertNoTaskScheduled(
                "If the notifications were ignored, the task should not reschedule itself.");
        assertNotificationNotShown();
    }

    @Test
    public void contentCheckFailedPreventsNotificationShow() {
        // Set up the callback to return empty string as if there were no fresh content in reality.
        mContentHost = "";

        PrefetchPrefs.setHasNewPages(true);
        setupDeviceOnlineStatus(false);

        // Run the task almost enough times.
        for (int i = 0; i < OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS - 1; i++) {
            runTask();
            assertNativeDidNotStart();
        }
        runTaskAndExpectTaskDone();
        assertNativeStarted();
        assertNotificationNotShown();
    }

    @Test
    public void testSettingClockBack() {
        // Set up the prefs so that a notification is shown.
        PrefetchPrefs.setHasNewPages(true);
        PrefetchPrefs.setOfflineCounter(OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS);
        setupDeviceOnlineStatus(false);
        runTaskAndExpectTaskDone();
        assertTrue(PrefetchPrefs.getNotificationLastShownTime() > 0);

        // Then set new content and change back the clock.
        PrefetchPrefs.setHasNewPages(true);
        mCalendar.add(Calendar.DATE, -20);

        // Schedule a task.  We expect a delay for tomorrow morning.
        OfflineNotificationBackgroundTask.scheduleTask(
                OfflineNotificationBackgroundTask.DETECTION_MODE_ONLINE);
        assertTaskScheduledForTomorrowMorning(
                "The delay should be for tomorrow morning even when the notification was last "
                + " shown in the future.");

        // Wait two days.  We should only have the offline delay, even though initially the
        // notification last shown time was for 20 days in the future.
        mCalendar.add(Calendar.DATE, 2);
        runTask();
        assertTaskScheduledForOfflineDelay(
                "After waiting for tomorrow morning, the next delay should be normal "
                + "even if the last notification was sent well in the future.");
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O)
    public void disabledPrefDoesNothingSdkO() {
        // Set up the prefs so that a notification should be shown.
        PrefetchPrefs.setHasNewPages(true);
        PrefetchPrefs.setOfflineCounter(OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS);
        PrefetchPrefs.setIgnoredNotificationCounter(0);
        setupDeviceOnlineStatus(false);

        // Set up the Content Suggestions notifications preference.
        PrefetchPrefs.setNotificationEnabled(false);

        runTaskAndExpectTaskDone();
        assertNativeStarted();
        assertNotificationShown();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void disabledPrefPreventsNotificationShow() {
        // Set up the prefs so that a notification would be shown, if not for the Content
        // Suggestions notifications preference.
        PrefetchPrefs.setHasNewPages(true);
        PrefetchPrefs.setOfflineCounter(OfflineNotificationBackgroundTask.OFFLINE_POLLING_ATTEMPTS);
        PrefetchPrefs.setIgnoredNotificationCounter(0);
        setupDeviceOnlineStatus(false);

        // Set up the Content Suggestions notifications preference.
        PrefetchPrefs.setNotificationEnabled(false);

        runTask();
        assertNativeDidNotStart();
        assertNoTaskScheduled("If the notifications preference was disabled, the task should not "
                + "reschedule itself.");
        assertNotificationNotShown();
    }
}
