// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class ActorMetricsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_ID = 77;

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private Profile mOriginalProfile;
    @Mock private ActorKeyedService mActorService;

    private ActorMetrics mActorMetrics;

    @Before
    public void setUp() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mOriginalProfile);
        ActorKeyedServiceFactory.setForTesting(mActorService);
        ActorMetrics.resetForTesting();
        mActorMetrics = ActorMetrics.getInstance();
    }

    @After
    public void tearDown() {
        ActorMetrics.resetForTesting();
    }

    @Test
    public void testMaybeRecordMetricsFromIntent_PrioritizeService() {
        int taskId = 123;
        int intentState = ActorTaskState.ACTING;
        int serviceState = ActorTaskState.REFLECTING;

        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(serviceState);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, intentState);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", serviceState);

        ActorMetrics.maybeRecordMetricsFromIntent(intent, mProfile);

        watcher.assertExpected();
        // Extras must NOT be stripped, so downstream consumers can still read them.
        assertTrue(intent.hasExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID));
        assertTrue(intent.hasExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE));

        // Re-invoking with the same intent instance must be deduplicated and not record again.
        var duplicateWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Notification.ClickTaskState")
                        .build();
        ActorMetrics.maybeRecordMetricsFromIntent(intent, mProfile);
        duplicateWatcher.assertExpected();
    }

    @Test
    public void testMaybeRecordMetricsFromIntent_FallbackToIntent() {
        int taskId = 456;
        int intentState = ActorTaskState.WAITING_ON_USER;

        // Service does not have the task.
        when(mActorService.getTask(taskId)).thenReturn(null);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, intentState);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", intentState);

        ActorMetrics.maybeRecordMetricsFromIntent(intent, mProfile);

        watcher.assertExpected();
    }

    @Test
    public void testMaybeRecordMetricsFromIntent_MultipleTasksInService() {
        int taskId1 = 101;
        int state1 = ActorTaskState.ACTING;
        int taskId2 = 102;
        int state2 = ActorTaskState.WAITING_ON_USER;

        ActorTask mockTask1 = mock(ActorTask.class);
        when(mockTask1.getState()).thenReturn(state1);
        when(mActorService.getTask(taskId1)).thenReturn(mockTask1);

        ActorTask mockTask2 = mock(ActorTask.class);
        when(mockTask2.getState()).thenReturn(state2);
        when(mActorService.getTask(taskId2)).thenReturn(mockTask2);

        // Click on Task 2's notification.
        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId2);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, ActorTaskState.CREATED);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", state2);

        ActorMetrics.maybeRecordMetricsFromIntent(intent, mProfile);

        watcher.assertExpected();
    }

    @Test
    public void testTaskStateTransitionsInForeground() {
        int taskId = 123;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";
        String histogramForegroundReflecting =
                "Actor.Tools.ExecutionDuration.Foreground.Reflecting";

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.REFLECTING);
        ShadowSystemClock.advanceBy(300, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 500)
                        .expectIntRecord(histogramForegroundReflecting, 300)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testPiPTransitions() {
        int taskId = 456;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";
        String histogramPipActing = "Actor.Tools.ExecutionDuration.Pip.Acting";

        // Start in Foreground
        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(400, TimeUnit.MILLISECONDS);

        // Transition to PiP while still ACTING
        mActorMetrics.setIsInPip(true);
        ShadowSystemClock.advanceBy(600, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 400)
                        .expectIntRecord(histogramPipActing, 600)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testMultipleTasks() {
        int taskId1 = 1;
        int taskId2 = 2;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";

        mActorMetrics.onTaskStateChangedForTesting(taskId1, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId2, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        // taskId1: 100ms (alone) + 200ms (while taskId2 is also ACTING) = 300ms
        // taskId2: 200ms

        var watcher1 = HistogramWatcher.newSingleRecordWatcher(histogramForegroundActing, 300);
        mActorMetrics.onTaskStateChangedForTesting(taskId1, ActorTaskState.FINISHED);
        watcher1.assertExpected();

        var watcher2 = HistogramWatcher.newSingleRecordWatcher(histogramForegroundActing, 200);
        mActorMetrics.onTaskStateChangedForTesting(taskId2, ActorTaskState.FINISHED);
        watcher2.assertExpected();
    }

    @Test
    public void testAccumulateMultipleTimesSameState() {
        int taskId = 789;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.PAUSED_BY_USER);
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 300)
                        .expectIntRecord(
                                "Actor.Tools.ExecutionDuration.Foreground.PausedByUser", 500)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testRecordOmniboxFocus_withActiveTaskOnTab() {
        int taskId = 101;
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(TAB_ID, /* includePaused= */ true))
                .thenReturn(taskId);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);

        var focusWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Ui.OmniboxClick.TaskState", ActorTaskState.ACTING);

        ActorMetrics.recordOmniboxFocus(mTab);
        focusWatcher.assertExpected();

        assertEquals(1, mActorMetrics.getOmniboxClickCountForTesting(taskId));

        // Focus a second time in REFLECTING state.
        when(mockTask.getState()).thenReturn(ActorTaskState.REFLECTING);
        var focusWatcher2 =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Ui.OmniboxClick.TaskState", ActorTaskState.REFLECTING);

        ActorMetrics.recordOmniboxFocus(mTab);
        focusWatcher2.assertExpected();
        assertEquals(2, mActorMetrics.getOmniboxClickCountForTesting(taskId));

        // Finish the task and assert OmniboxClickCount.Completed is emitted with 2.
        var completionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Task.OmniboxClickCount.Completed", 2);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        completionWatcher.assertExpected();
        assertEquals(0, mActorMetrics.getOmniboxClickCountForTesting(taskId));
    }

    @Test
    public void testRecordOmniboxFocus_taskBoundToOtherTabIgnored() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(TAB_ID, /* includePaused= */ true))
                .thenReturn(null);

        var focusWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Ui.OmniboxClick.TaskState")
                        .build();

        ActorMetrics.recordOmniboxFocus(mTab);
        focusWatcher.assertExpected();
    }

    @Test
    public void testRecordOmniboxFocus_noActiveTask() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);

        var focusWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Ui.OmniboxClick.TaskState")
                        .build();

        ActorMetrics.recordOmniboxFocus(mTab);
        focusWatcher.assertExpected();
    }

    @Test
    public void testRecordOmniboxFocus_nullTab() {
        var focusWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Ui.OmniboxClick.TaskState")
                        .build();

        ActorMetrics.recordOmniboxFocus(null);
        focusWatcher.assertExpected();
    }

    @Test
    public void testOnTaskStopped_stoppedReasonRecorded() {
        int taskId = 202;
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(TAB_ID, /* includePaused= */ true))
                .thenReturn(taskId);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);

        ActorMetrics.recordOmniboxFocus(mTab);
        ActorMetrics.recordOmniboxFocus(mTab);
        ActorMetrics.recordOmniboxFocus(mTab);
        assertEquals(3, mActorMetrics.getOmniboxClickCountForTesting(taskId));

        var timeoutWatcher =
                HistogramWatcher.newSingleRecordWatcher("Actor.Task.OmniboxClickCount.Timeout", 3);

        mActorMetrics.onTaskStoppedForTesting(taskId, StoppedReason.TIMEOUT);
        timeoutWatcher.assertExpected();

        // Ensure that subsequent completed state change does not double-record.
        var cancelledWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Task.OmniboxClickCount.Cancelled")
                        .build();
        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.CANCELLED);
        cancelledWatcher.assertExpected();
    }

    @Test
    public void testOnTaskStopped_userNavigatedAway() {
        int taskId = 404;
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(TAB_ID, /* includePaused= */ true))
                .thenReturn(taskId);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);

        ActorMetrics.recordOmniboxFocus(mTab);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Task.OmniboxClickCount.UserNavigatedAway", 1);
        mActorMetrics.onTaskStoppedForTesting(taskId, StoppedReason.USER_NAVIGATED_AWAY);
        watcher.assertExpected();
    }

    @Test
    public void testTaskCompletedWithZeroClicks() {
        int taskId = 303;
        var completionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Task.OmniboxClickCount.Completed", 0);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        completionWatcher.assertExpected();
    }

    @Test
    public void testOnTaskStateChangedFollowedByOnTaskStopped_noDuplicateRecord() {
        int taskId = 505;
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(TAB_ID, /* includePaused= */ true))
                .thenReturn(taskId);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);

        ActorMetrics.recordOmniboxFocus(mTab);
        assertEquals(1, mActorMetrics.getOmniboxClickCountForTesting(taskId));

        var cancelledWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Task.OmniboxClickCount.Cancelled", 1);
        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.CANCELLED);
        cancelledWatcher.assertExpected();

        // Subsequent onTaskStopped (e.g. from JNI or observer dispatch) must not record again.
        var duplicateWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Task.OmniboxClickCount.Cancelled")
                        .expectNoRecords("Actor.Task.OmniboxClickCount.Timeout")
                        .build();
        mActorMetrics.onTaskStoppedForTesting(taskId, StoppedReason.TIMEOUT);
        duplicateWatcher.assertExpected();
    }

    @Test
    public void testRecordOmniboxFocus_tabDestroyedIgnored() {
        Tab destroyedTab = mock(Tab.class);
        when(destroyedTab.isDestroyed()).thenReturn(true);

        var focusWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Ui.OmniboxClick.TaskState")
                        .build();

        ActorMetrics.recordOmniboxFocus(destroyedTab);
        focusWatcher.assertExpected();
    }

    @Test
    public void testRecordOmniboxFocus_offTheRecordIgnored() {
        Profile otrProfile = mock(Profile.class);
        when(otrProfile.isOffTheRecord()).thenReturn(true);
        Tab otrTab = mock(Tab.class);
        when(otrTab.isDestroyed()).thenReturn(false);
        when(otrTab.getProfile()).thenReturn(otrProfile);

        var focusWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.Ui.OmniboxClick.TaskState")
                        .build();

        ActorMetrics.recordOmniboxFocus(otrTab);
        focusWatcher.assertExpected();
    }
}
