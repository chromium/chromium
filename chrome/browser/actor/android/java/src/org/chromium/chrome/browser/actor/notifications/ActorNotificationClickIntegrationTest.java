// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityOptions;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.actor.ActorForegroundServiceControllerImpl;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorMetrics.ActorFetchConversationIdStatus;
import org.chromium.chrome.browser.actor.ActorMetrics.ActorFetchTabIdStatus;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskJni;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicIntentConstants;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for actor notification clicks. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests intent handling which involves activity startup.")
public class ActorNotificationClickIntegrationTest {
    private static final int ROUTED_TASK_ID = 301;
    private static final String GLIC_CONVERSATION_ID = "notification-conversation";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String ROUTED_CONVERSATION_ID = "conv-routed";

    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask.Natives mActorTaskJni;
    @Mock private ActorTask mRoutedTask;
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
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testNotificationClickWarmStart_FocusesTaskTabAndInvokesGlic() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        Tab otherTab = mActivityTestRule.getActivityTab();
        Tab taskTab =
                mActivityTestRule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        LayoutManager layoutManager = activity.getLayoutManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = activity.getTabModelSelector().getModel(false);
                    TabModelUtils.setIndex(model, model.indexOf(otherTab));
                    layoutManager.showLayout(LayoutType.HUB, /* animate= */ false);
                });
        CriteriaHelper.pollUiThread(() -> layoutManager.isLayoutVisible(LayoutType.HUB));

        Intent intent = createNotificationIntentForTab(ROUTED_TASK_ID, taskTab.getId());
        ThreadUtils.runOnUiThreadBlocking(() -> activity.onNewIntent(intent));

        waitForTabFocusedInBrowsingLayout(activity, taskTab.getId());
        verifyGlicInvokedForTab(taskTab.getId());
    }

    /**
     * On cold start the task's tab is only known by ID until tab state restoration adds it, so
     * focusing it and invoking Glic must wait for the restored tab rather than act on whatever tab
     * happens to be current.
     */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testNotificationClickColdStart_FocusesRestoredTaskTabAndInvokesGlic()
            throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity previousActivity = mActivityTestRule.getActivity();
        Tab otherTab = mActivityTestRule.getActivityTab();
        int taskTabId =
                mActivityTestRule
                        .loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)
                        .getId();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = previousActivity.getTabModelSelector().getModel(false);
                    TabModelUtils.setIndex(model, model.indexOf(otherTab));
                    previousActivity.saveState();
                });
        ApplicationTestUtils.finishActivity(previousActivity);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        mActivityTestRule.startMainActivityFromIntent(
                createNotificationIntentForTab(ROUTED_TASK_ID, taskTabId), null);

        waitForTabFocusedInBrowsingLayout(mActivityTestRule.getActivity(), taskTabId);
        verifyGlicInvokedForTab(taskTabId);
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

        CountingNotificationManagerProxy notificationManager =
                new CountingNotificationManagerProxy();
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
        // A warm click reaches both onNewIntentWithNative() and maybeHandleSingleUrlIntent(), and
        // each dismisses the notification. The routing migration dismisses it once.
        CriteriaHelper.pollUiThread(() -> notificationManager.getCancelCount(taskId) >= 2);
        ThreadUtils.runOnUiThreadBlocking(() -> {});
        assertEquals(
                2,
                (int)
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> notificationManager.getCancelCount(taskId)));
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

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    public void testNotificationTapThroughLauncher_SelectsActingTabAndOpensGlic() throws Exception {
        int taskId = 301;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.ACTING, actingTab);
        var watcher =
                expectRoutedTapHistograms(ActorTaskState.ACTING, /* expectConversationId= */ true);
        var bringToFrontWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Intent.BringTabToFront.Result",
                        ChromeLauncherActivity.BringTabToFrontResult.SUCCESS);

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        waitForSelectedTab(actingTab);
        verifyGlicOpenedOn(actingTab);
        watcher.assertExpected();
        bringToFrontWatcher.assertExpected();
        assertNotNull(
                "Tapping a running task must keep its notification",
                getPostedNotification(notificationManager, taskId));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    public void testNotificationTapThroughLauncher_RoutingFlagOff_SelectsTabWithoutGlic()
            throws Exception {
        int taskId = 303;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.ACTING, actingTab);
        var watcher =
                expectRoutedTapHistograms(ActorTaskState.ACTING, /* expectConversationId= */ false);

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        waitForSelectedTab(actingTab);
        // ActorTabStateHelper#listenAndSelectTabOnAdded runs its callback, which would open Glic,
        // synchronously right after selecting the tab, so one flush is enough.
        ThreadUtils.runOnUiThreadBlocking(() -> {});
        verify(mGlicKeyedService, never()).invokeWithConversation(any(), anyString(), anyInt());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING,
        ChromeFeatureList.MOVE_TO_FRONT_IN_LAUNCH_INTENT_DISPATCHER
    })
    public void testUntrustedIntentWithActorExtras_ToLauncher_DoesNotRoute() throws Exception {
        int taskId = 305;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.FINISHED, actingTab);
        Tab selectedTab = mActivityTestRule.getActivityTab();
        // Spoofed intents with actor extras still record the click.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.Notification.ClickTaskState", ActorTaskState.FINISHED)
                        .expectNoRecords("Actor.Notification.FetchTabId.Status")
                        .expectNoRecords("Android.Intent.BringTabToFront.Result")
                        .build();

        Intent intent =
                new Intent()
                        .setClass(mContext, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(IntentHandler.BRING_TAB_TO_FRONT_EXTRA, actingTab.getId())
                        .putExtra(
                                IntentHandler.BRING_TAB_TO_FRONT_SOURCE_EXTRA,
                                IntentHandler.BringToFrontSource.NOTIFICATION)
                        .putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true)
                        .putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId)
                        .putExtra(
                                NotificationConstants.EXTRA_ACTOR_TASK_STATE,
                                ActorTaskState.FINISHED)
                        .putExtra(
                                GlicIntentConstants.EXTRA_CONVERSATION_ID, ROUTED_CONVERSATION_ID);
        mContext.startActivity(intent);

        // The rest of the intent is handled synchronously after the click is recorded.
        watcher.pollInstrumentationThreadUntilSatisfied();
        ThreadUtils.runOnUiThreadBlocking(() -> {});
        watcher.assertExpected();
        assertEquals(selectedTab, mActivityTestRule.getActivityTab());
        verify(mGlicKeyedService, never()).invokeWithConversation(any(), anyString(), anyInt());
        assertNotNull(
                "Spoofed intent must not dismiss the notification",
                findPostedNotification(notificationManager, taskId));
    }

    @Test
    @MediumTest
    public void testNotificationTapAfterActivityFinished_RestoresTabAndRecordsNotificationLaunch()
            throws Exception {
        int taskId = 306;
        Tab actingTab = startWithActingTabInBackground();
        int actingTabId = actingTab.getId();
        // Finishing the last activity starts a foreground service whose notification needs a real
        // channel, which the mocked notification manager would not create.
        Resources resources = mContext.getResources();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        new ChannelsInitializer(
                                        new NotificationManagerProxyImpl(),
                                        ChromeChannelDefinitions.getInstance(),
                                        resources)
                                .safeInitialize(ChromeChannelDefinitions.ChannelId.BROWSER));
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.ACTING, actingTab);
        Notification notification = getPostedNotification(notificationManager, taskId);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "MobileStartup.LaunchCause", LaunchCauseMetrics.LaunchCause.NOTIFICATION);

        ActivityOptions options = ActivityOptions.makeBasic();
        ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartAllowAlways(options);
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> {
                            try {
                                notification.contentIntent.send(
                                        mContext, 0, null, null, null, null, options.toBundle());
                            } catch (PendingIntent.CanceledException e) {
                                throw new AssertionError(e);
                            }
                        });
        mActivityTestRule.setActivity(activity);
        mActivityTestRule.waitForActivityCompletelyLoaded();

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = mActivityTestRule.getActivityTab();
                    return tab != null && tab.getId() == actingTabId;
                },
                "The restored acting tab was not selected");
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    public void testNotificationTap_FinishedTaskGone_RoutesToStampedTabAndDismisses()
            throws Exception {
        int taskId = 311;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.FINISHED, actingTab);
        when(mActorKeyedService.getTask(taskId)).thenReturn(null);
        when(mActorKeyedService.getTaskByConversationId(ROUTED_CONVERSATION_ID)).thenReturn(null);

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        waitForSelectedTab(actingTab);
        verifyGlicInvokedOn(actingTab);
        CriteriaHelper.pollUiThread(
                () -> findPostedNotification(notificationManager, taskId) == null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    public void testNotificationTap_FinishedTaskGoneAndTabClosed_OpensGlicOnSelectedTab()
            throws Exception {
        int taskId = 313;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.FINISHED, actingTab);
        when(mActorKeyedService.getTask(taskId)).thenReturn(null);
        when(mActorKeyedService.getTaskByConversationId(ROUTED_CONVERSATION_ID)).thenReturn(null);
        Tab selectedTab = mActivityTestRule.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getCurrentTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(actingTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.FetchTabId.Status",
                        ActorFetchTabIdStatus.TAB_NOT_FOUND);

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        watcher.pollInstrumentationThreadUntilSatisfied();
        ThreadUtils.runOnUiThreadBlocking(() -> {});
        assertEquals(selectedTab, mActivityTestRule.getActivityTab());
        // With the task's tab gone, Glic falls back to the selected regular tab.
        verifyGlicInvokedOn(selectedTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNotificationTap_IncognitoTabSelected_SelectsActingTabAndOpensGlic()
            throws Exception {
        int taskId = 321;
        Tab actingTab = startWithActingTabInBackground();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.ACTING, actingTab);
        mActivityTestRule.newIncognitoTabFromMenu();

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        waitForSelectedTab(actingTab);
        verifyGlicInvokedOn(actingTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNotificationTap_TabClosedWhileIncognitoSelected_OpensGlicOnRegularTab()
            throws Exception {
        int taskId = 322;
        Tab actingTab = startWithActingTabInBackground();
        Tab regularTab = mActivityTestRule.getActivityTab();
        MockNotificationManagerProxy notificationManager =
                postRoutedTaskNotification(taskId, ActorTaskState.ACTING, actingTab);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getCurrentTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(actingTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
        mActivityTestRule.newIncognitoTabFromMenu();
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Notification.FetchTabId.Status",
                        ActorFetchTabIdStatus.TAB_NOT_FOUND);

        getPostedNotification(notificationManager, taskId).contentIntent.send();

        // With the task's tab gone, Glic can't open on the selected incognito tab, so it falls back
        // to the selected regular tab.
        verifyGlicInvokedOn(regularTab);
        assertEquals(regularTab, mActivityTestRule.getActivityTab());
        watcher.assertExpected();
    }

    /**
     * Counts how often each untagged notification is cancelled. A Mockito spy would also record
     * calls made from background threads, whose callbacks keep the activity alive and fail leak
     * checks.
     */
    private static class CountingNotificationManagerProxy extends MockNotificationManagerProxy {
        private final Map<Integer, Integer> mCancelCounts = new HashMap<>();

        @Override
        public void cancel(String tag, int id) {
            if (tag == null) mCancelCounts.merge(id, 1, Integer::sum);
            super.cancel(tag, id);
        }

        int getCancelCount(int id) {
            return mCancelCounts.getOrDefault(id, 0);
        }
    }

    /**
     * Starts the activity with three tabs and returns the second one. The third tab stays selected,
     * so a tap that changes nothing is caught.
     */
    private Tab startWithActingTabInBackground() {
        mActivityTestRule.startMainActivityOnBlankPage();
        Tab actingTab = mActivityTestRule.loadUrlInNewTab("about:blank");
        mActivityTestRule.loadUrlInNewTab("about:blank");
        return actingTab;
    }

    /**
     * Posts the notification for a real task acting on {@code actingTab} through {@link
     * ActorNotificationService}, so that its PendingIntents are the ones a user taps.
     */
    private MockNotificationManagerProxy postRoutedTaskNotification(
            int taskId, @ActorTaskState int state, Tab actingTab) {
        MockNotificationManagerProxy notificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(notificationManager);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        ActorTaskJni.setInstanceForTesting(mActorTaskJni);

        // The task ID doubles as the fake native pointer.
        long nativeTaskPtr = taskId;
        when(mActorTaskJni.getState(nativeTaskPtr)).thenReturn(state);
        when(mActorTaskJni.getLastActuatedTabId(nativeTaskPtr)).thenReturn(actingTab.getId());
        when(mActorTaskJni.getTabs(nativeTaskPtr)).thenReturn(new int[] {actingTab.getId()});
        when(mActorTaskJni.getCurrentActionName(nativeTaskPtr)).thenReturn("");
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getProfile(/* incognito= */ false));
        ActorTask task =
                new ActorTask(
                        nativeTaskPtr,
                        taskId,
                        "Routed task",
                        profile,
                        ROUTED_CONVERSATION_ID,
                        /* isExperimentalTriggering= */ false);
        when(mActorKeyedService.getTask(taskId)).thenReturn(task);
        when(mActorKeyedService.getTaskByConversationId(ROUTED_CONVERSATION_ID)).thenReturn(task);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        new ActorNotificationService(mActorKeyedService)
                                .updateNotificationForTask(
                                        taskId,
                                        state,
                                        /* isSilent= */ true,
                                        /* isWarning= */ false));
        return notificationManager;
    }

    private static Notification getPostedNotification(
            MockNotificationManagerProxy notificationManager, int taskId) {
        Notification notification = findPostedNotification(notificationManager, taskId);
        assertNotNull("Task notification was not posted", notification);
        return notification;
    }

    private static Notification findPostedNotification(
            MockNotificationManagerProxy notificationManager, int taskId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (NotificationEntry entry : notificationManager.getNotifications()) {
                        if (entry.id == taskId) return entry.notification;
                    }
                    return null;
                });
    }

    private static HistogramWatcher expectRoutedTapHistograms(
            @ActorTaskState int state, boolean expectConversationId) {
        var builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Actor.Notification.ClickTaskState", state)
                        .expectIntRecord(
                                "Actor.Notification.FetchTabId.Status",
                                ActorFetchTabIdStatus.SUCCESS);
        if (expectConversationId) {
            builder.expectIntRecord(
                    "Actor.Notification.FetchConversationId.Status",
                    ActorFetchConversationIdStatus.SUCCESS);
        } else {
            builder.expectNoRecords("Actor.Notification.FetchConversationId.Status");
        }
        return builder.build();
    }

    private void waitForSelectedTab(Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivityTab() == tab, "The acting tab was not selected");
    }

    /** Verifies Glic opened once, on the task's conversation, whatever the invocation source. */
    private void verifyGlicInvokedOn(Tab tab) {
        verify(mGlicKeyedService, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .invokeWithConversation(eq(tab), eq(ROUTED_CONVERSATION_ID), anyInt());
        verify(mGlicKeyedService).invokeWithConversation(any(), anyString(), anyInt());
    }

    private void verifyGlicOpenedOn(Tab tab) {
        // Notification taps report TOOLBAR_BUTTON today. The routing migration moves them to an
        // actor invocation source and updates this expectation.
        verify(mGlicKeyedService, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .invokeWithConversation(
                        tab, ROUTED_CONVERSATION_ID, GlicInvocationSource.TOOLBAR_BUTTON);
    }

    /** Builds the intent a notification tap delivers, via the production intent factory. */
    private Intent createNotificationIntentForTab(int taskId, int tabId) {
        when(mRoutedTask.getId()).thenReturn(taskId);
        when(mRoutedTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        when(mRoutedTask.getTargetTabId()).thenReturn(tabId);
        when(mRoutedTask.getGlicConversationId()).thenReturn(GLIC_CONVERSATION_ID);
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        new ActorForegroundServiceControllerImpl()
                                .createTrustedBringTabToFrontIntent(mRoutedTask));
    }

    private static void waitForTabFocusedInBrowsingLayout(
            ChromeTabbedActivity activity, int expectedTabId) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab currentTab = activity.getActivityTab();
                    Criteria.checkThat(
                            currentTab == null ? Tab.INVALID_TAB_ID : currentTab.getId(),
                            Matchers.is(expectedTabId));
                    LayoutManager layoutManager = activity.getLayoutManager();
                    Criteria.checkThat(
                            layoutManager.isLayoutVisible(LayoutType.HUB), Matchers.is(false));
                    Criteria.checkThat(
                            layoutManager.isLayoutVisible(LayoutType.BROWSING), Matchers.is(true));
                });
    }

    private void verifyGlicInvokedForTab(int taskTabId) {
        verify(mGlicKeyedService, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .invokeWithConversation(
                        argThat(tab -> tab != null && tab.getId() == taskTabId),
                        eq(GLIC_CONVERSATION_ID),
                        anyInt());
        verify(mGlicKeyedService).invokeWithConversation(any(), any(), anyInt());
    }
}
