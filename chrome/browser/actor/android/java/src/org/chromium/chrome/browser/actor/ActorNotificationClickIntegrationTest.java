// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Integration tests for actor notification clicks. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests intent handling which involves activity startup.")
public class ActorNotificationClickIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mActorKeyedService;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @After
    public void tearDown() {
        ActorForegroundServiceController.setInstanceForTesting(null);
        ActorKeyedServiceFactory.setForTesting(null);
    }

    @Test
    @MediumTest
    public void testNotificationClickColdStart_RecordsHistogram() throws Exception {
        int taskId = 123;
        int state = ActorTaskState.ACTING;

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher("Actor.Notification.ClickTaskState", state);

        mActivityTestRule.startMainActivityFromIntent(intent, null);

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testNotificationClickWarmStart_RecordsHistogram() throws Exception {
        int taskId = 456;
        int state = ActorTaskState.WAITING_ON_USER;

        mActivityTestRule.startMainActivityOnBlankPage();

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher("Actor.Notification.ClickTaskState", state);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onNewIntent(intent));

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testNotificationClick_UsingIntentFromService_RecordsHistogram() throws Exception {
        int taskId = 789;
        int state = ActorTaskState.ACTING;

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(taskId);
        when(task.getState()).thenReturn(state);

        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(controller);

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);
        when(controller.createTrustedBringTabToFrontIntent(task)).thenReturn(intent);

        // Verify that building a notification for this task produces a PendingIntent.
        var wrapper = ActorNotificationFactory.buildNotification(task, state, false, false);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher("Actor.Notification.ClickTaskState", state);

        // Simulate the notification click.
        mActivityTestRule.startMainActivityFromIntent(intent, null);

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testNotificationClick_WithLiveTask_RecordsHistogram() throws Exception {
        int taskId = 101;
        int intentState = ActorTaskState.CREATED;
        int liveState = ActorTaskState.ACTING;

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        ActorTask liveTask = mock(ActorTask.class);
        when(mActorKeyedService.getTask(taskId)).thenReturn(liveTask);
        when(liveTask.getState()).thenReturn(liveState);

        // The intent contains an older state, but the live state should be logged.
        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, intentState);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", liveState);

        // Simulate the notification click.
        mActivityTestRule.startMainActivityFromIntent(intent, null);

        watcher.assertExpected();
    }
}
