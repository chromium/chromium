// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

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
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.actor.ActorForegroundServiceControllerImpl;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskJni;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for actor notification clicks. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests intent handling which involves activity startup.")
public class ActorNotificationClickIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask.Natives mActorTaskJni;
    @Mock private GlicKeyedService mGlicKeyedService;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @After
    public void tearDown() {
        ActorForegroundServiceController.setInstanceForTesting(null);
        ActorForegroundServiceManager.resetInstanceForTesting();
        ActorKeyedServiceFactory.setForTesting(null);
        ActorTaskJni.setInstanceForTesting(null);
        GlicKeyedServiceFactory.setForTesting(null);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
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
        IntentUtils.addTrustedIntentExtras(intent);

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
        IntentUtils.addTrustedIntentExtras(intent);

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
        when(task.getTargetTabId()).thenReturn(Tab.INVALID_TAB_ID);

        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(controller);

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);
        IntentUtils.addTrustedIntentExtras(intent);
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
        when(liveTask.getTargetTabId()).thenReturn(Tab.INVALID_TAB_ID);

        // The intent contains an older state, but the live state should be logged.
        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, intentState);
        IntentUtils.addTrustedIntentExtras(intent);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", liveState);

        // Simulate the notification click.
        mActivityTestRule.startMainActivityFromIntent(intent, null);

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testNotificationClick_CompletedTask_DismissesNotification() throws Exception {
        int taskId = 105;
        int state = ActorTaskState.FINISHED;

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        notificationManager.notify(taskId, new Notification());
        assertEquals(1, notificationManager.getNotifications().size());

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);
        IntentUtils.addTrustedIntentExtras(intent);

        mActivityTestRule.startMainActivityFromIntent(intent, null);

        CriteriaHelper.pollUiThread(() -> notificationManager.getNotifications().isEmpty());
    }

    @Test
    @MediumTest
    public void testNotificationClickWarmStart_CompletedTask_DismissesNotification()
            throws Exception {
        int taskId = 106;
        int state = ActorTaskState.FINISHED;

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        notificationManager.notify(taskId, new Notification());
        assertEquals(1, notificationManager.getNotifications().size());

        mActivityTestRule.startMainActivityOnBlankPage();

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);
        IntentUtils.addTrustedIntentExtras(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onNewIntent(intent));

        CriteriaHelper.pollUiThread(() -> notificationManager.getNotifications().isEmpty());
    }

    @Test
    @MediumTest
    public void testNotificationClick_UntrustedIntent_DoesNotDismissNotification()
            throws Exception {
        int taskId = 108;
        int state = ActorTaskState.FINISHED;

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        notificationManager.notify(taskId, new Notification());
        assertEquals(1, notificationManager.getNotifications().size());

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        mActivityTestRule.startMainActivityFromIntent(intent, null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onNewIntent(intent));

        assertEquals(1, notificationManager.getNotifications().size());
    }

    @Test
    @MediumTest
    public void testNotificationClick_ActiveTask_DoesNotDismissNotification() throws Exception {
        int taskId = 107;
        int state = ActorTaskState.ACTING;

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        notificationManager.notify(taskId, new Notification());
        assertEquals(1, notificationManager.getNotifications().size());

        Intent intent = new Intent(mContext, ChromeTabbedActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);
        IntentUtils.addTrustedIntentExtras(intent);

        mActivityTestRule.startMainActivityFromIntent(intent, null);

        assertEquals(1, notificationManager.getNotifications().size());
    }

    /**
     * A task's native counterpart is destroyed as soon as it completes, after which the task can no
     * longer report a live state. The live notification must still be demoted to an ordinary,
     * dismissible one, otherwise it stays pinned indefinitely.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.BAKLAVA)
    @EnableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
    public void testCompletedTaskWithUnreadableState_DemotesLiveNotification() throws Exception {
        int taskId = 202;

        mActivityTestRule.startMainActivityOnBlankPage();

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        ActorNotificationService.setDemotionDelayMsForTesting(100);
        ActorForegroundServiceController.setInstanceForTesting(
                mock(ActorForegroundServiceController.class));

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(taskId);
        when(task.getTitle()).thenReturn("Integration Task");
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorKeyedService.getTask(taskId)).thenReturn(task);

        ActorNotificationService service =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new ActorNotificationService(mActorKeyedService));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        service.updateNotificationForTask(
                                taskId,
                                ActorTaskState.FINISHED,
                                /* isSilent= */ false,
                                /* isWarning= */ false));

        // Native task is gone now, so the task object can only report a stale default.
        when(task.getState()).thenReturn(ActorTaskState.CREATED);

        CriteriaHelper.pollUiThread(
                () -> {
                    Notification notification =
                            service.getCachedNotification(
                                    taskId, /* isSilent= */ false, /* isWarning= */ false);
                    return notification != null
                            && (notification.flags & Notification.FLAG_ONGOING_EVENT) == 0;
                },
                "Live notification for a completed task was never demoted");
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.BAKLAVA)
    @EnableFeatures({
        ChromeFeatureList.ACTOR_LIVE_NOTIFICATION,
        ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING
    })
    public void
            testTapDemotedCompletedNotification_AfterNativeTaskRemoval_FocusesTabInvokesGlicAndDismisses()
                    throws Exception {
        long nativeTaskPtr = 1234L;
        int taskId = 203;
        String expectedConversationId = "conv-demoted-203";

        mActivityTestRule.startMainActivityOnBlankPage();
        Tab targetTab = mActivityTestRule.getActivityTab();
        int targetTabId = targetTab.getId();

        // Open a second tab so targetTab is not the active tab before the notification tap.
        mActivityTestRule.loadUrlInNewTab("about:blank");

        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        ActorNotificationService.setDemotionDelayMsForTesting(100);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        ActorTaskJni.setInstanceForTesting(mActorTaskJni);

        when(mActorTaskJni.getState(nativeTaskPtr)).thenReturn(ActorTaskState.FINISHED);
        when(mActorTaskJni.getLastActuatedTabId(nativeTaskPtr)).thenReturn(targetTabId);

        ActorForegroundServiceControllerImpl controller =
                spy(new ActorForegroundServiceControllerImpl());
        AtomicReference<Intent> lastCreatedIntent = new AtomicReference<>();
        doAnswer(
                        invocation -> {
                            Intent intent = (Intent) invocation.callRealMethod();
                            lastCreatedIntent.set(intent);
                            return intent;
                        })
                .when(controller)
                .createTrustedBringTabToFrontIntent(any());
        ActorForegroundServiceController.setInstanceForTesting(controller);

        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getProfile(/* incognito= */ false));
        ActorTask realTask =
                new ActorTask(
                        nativeTaskPtr,
                        taskId,
                        "Integration Task",
                        profile,
                        expectedConversationId,
                        /* isExperimentalTriggering= */ false);
        when(mActorKeyedService.getTask(taskId)).thenReturn(realTask);

        ActorNotificationService service =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new ActorNotificationService(mActorKeyedService));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    service.updateNotificationForTask(
                            taskId,
                            ActorTaskState.FINISHED,
                            /* isSilent= */ false,
                            /* isWarning= */ false);
                    // The native task is destroyed and removed from ActorKeyedService before the
                    // demotion timer fires, so the tap path must fall back to the intent's state.
                    realTask.clearNativePtr();
                    when(mActorKeyedService.getTask(taskId)).thenReturn(null);
                    lastCreatedIntent.set(null);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Notification notification =
                            service.getCachedNotification(
                                    taskId, /* isSilent= */ false, /* isWarning= */ false);
                    return notification != null
                            && (notification.flags & Notification.FLAG_ONGOING_EVENT) == 0;
                },
                "Live notification for a completed task was never demoted");

        Intent demotedIntent = lastCreatedIntent.get();
        assertNotNull(demotedIntent);
        assertEquals(targetTabId, IntentHandler.getBringTabToFrontId(demotedIntent));
        assertEquals(expectedConversationId, IntentHandler.getGlicConversationId(demotedIntent));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.ClickTaskState", ActorTaskState.FINISHED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onNewIntent(demotedIntent));

        watcher.assertExpected();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivityTab().getId() == targetTabId);
        verify(mGlicKeyedService, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .invokeWithConversation(eq(targetTab), eq(expectedConversationId), anyInt());
        CriteriaHelper.pollUiThread(() -> notificationManager.getNotifications().isEmpty());
    }
}
