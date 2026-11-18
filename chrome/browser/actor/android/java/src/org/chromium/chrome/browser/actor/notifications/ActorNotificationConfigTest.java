// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationConfig.NotificationState;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationConfig.TaskSnapshot;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationConfig.Trigger;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Unit tests for {@link ActorNotificationConfig}. Every expectation encodes how {@link
 * ActorNotificationService} and {@link ActorNotificationFactory} build notifications today.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.BAKLAVA)
@EnableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
public class ActorNotificationConfigTest {
    private static final int TASK_ID = 42;
    private static final int TAB_ID = 7;
    private static final String TASK_TITLE = "Task";
    private static final String STEP = "Clicking";
    private static final String CONVERSATION_ID = "conversation";
    private static final @ActorTaskState int[] ALL_TASK_STATES = {
        ActorTaskState.CREATED,
        ActorTaskState.ACTING,
        ActorTaskState.REFLECTING,
        ActorTaskState.PAUSED_BY_ACTOR,
        ActorTaskState.PAUSED_BY_USER,
        ActorTaskState.CANCELLED,
        ActorTaskState.FINISHED,
        ActorTaskState.WAITING_ON_USER,
        ActorTaskState.FAILED,
    };

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorTask mTask;
    @Mock private ActorForegroundServiceController mServiceController;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        ActorForegroundServiceController.setInstanceForTesting(mServiceController);
    }

    private static TaskSnapshot snapshot(@ActorTaskState int taskState) {
        return snapshot(taskState, /* stepProgressText= */ null, /* isExperimental= */ false);
    }

    private static TaskSnapshot snapshot(
            @ActorTaskState int taskState,
            @Nullable String stepProgressText,
            boolean isExperimental) {
        return new TaskSnapshot(
                TASK_ID,
                TASK_TITLE,
                stepProgressText,
                TAB_ID,
                CONVERSATION_ID,
                taskState,
                isExperimental);
    }

    private static TaskSnapshot experimentalSnapshot(@ActorTaskState int taskState) {
        return snapshot(taskState, /* stepProgressText= */ null, /* isExperimental= */ true);
    }

    private ActorNotificationConfig forTask(
            @Nullable ActorNotificationConfig previous,
            TaskSnapshot task,
            @Trigger int trigger,
            boolean isTabVisible) {
        return ActorNotificationConfig.forTask(
                mContext,
                previous,
                task,
                trigger,
                /* isLive= */ true,
                /* isLiveUpdatesEnabled= */ true,
                isTabVisible);
    }

    private ActorNotificationConfig forTask(
            @Nullable ActorNotificationConfig previous, TaskSnapshot task, @Trigger int trigger) {
        return forTask(previous, task, trigger, /* isTabVisible= */ false);
    }

    private ActorNotificationConfig forStateUpdate(
            @Nullable ActorNotificationConfig previous,
            @ActorTaskState int taskState,
            boolean isWarning) {
        return forTask(
                previous,
                snapshot(taskState),
                isWarning ? Trigger.TIMEOUT_WARNING : Trigger.TASK_STATE_CHANGED);
    }

    private void assertShows(
            ActorNotificationConfig config,
            @NotificationState int state,
            int titleRes,
            int bodyRes,
            int chipRes,
            boolean isHighPriority,
            boolean isTerminal) {
        assertEquals(state, config.getState());
        assertEquals(titleRes, config.getTitleRes());
        assertEquals(mContext.getString(bodyRes, TASK_TITLE), config.getBody());
        assertEquals(chipRes, config.getChipRes());
        assertEquals(isHighPriority, config.isHighPriority());
        assertEquals(isTerminal, config.isTerminal());
    }

    @Test
    public void testForTask_TaskStateChangedMapsStates() {
        ActorNotificationConfig created =
                forStateUpdate(
                        /* previous= */ null, ActorTaskState.CREATED, /* isWarning= */ false);
        assertShows(
                created,
                NotificationState.CREATED_PENDING_TAB,
                R.string.actor_notification_title_working_on_task,
                R.string.actor_notification_body_working,
                /* chipRes= */ 0,
                /* isHighPriority= */ false,
                /* isTerminal= */ false);

        ActorNotificationConfig acting =
                forStateUpdate(created, ActorTaskState.ACTING, /* isWarning= */ false);
        assertShows(
                acting,
                NotificationState.WORKING,
                R.string.actor_notification_title_working_on_task,
                R.string.actor_notification_body_working,
                /* chipRes= */ 0,
                /* isHighPriority= */ false,
                /* isTerminal= */ false);
        assertShows(
                forStateUpdate(acting, ActorTaskState.REFLECTING, /* isWarning= */ false),
                NotificationState.WORKING,
                R.string.actor_notification_title_working_on_task,
                R.string.actor_notification_body_working,
                /* chipRes= */ 0,
                /* isHighPriority= */ false,
                /* isTerminal= */ false);
        assertShows(
                forStateUpdate(acting, ActorTaskState.PAUSED_BY_USER, /* isWarning= */ false),
                NotificationState.PAUSED,
                R.string.actor_notification_title_task_paused,
                R.string.actor_notification_body_paused,
                R.string.actor_notification_live_status_paused,
                /* isHighPriority= */ false,
                /* isTerminal= */ false);
        assertShows(
                forStateUpdate(acting, ActorTaskState.WAITING_ON_USER, /* isWarning= */ false),
                NotificationState.WAITING_ON_USER,
                R.string.actor_notification_title_check_your_task,
                R.string.actor_notification_body_user_input,
                R.string.actor_notification_live_status_review,
                /* isHighPriority= */ true,
                /* isTerminal= */ false);
        assertShows(
                forStateUpdate(acting, ActorTaskState.FINISHED, /* isWarning= */ false),
                NotificationState.COMPLETED,
                R.string.actor_notification_title_task_complete,
                R.string.actor_notification_body_complete,
                R.string.actor_notification_live_status_done,
                /* isHighPriority= */ false,
                /* isTerminal= */ true);
        for (@ActorTaskState
        int taskState : new int[] {ActorTaskState.FAILED, ActorTaskState.CANCELLED}) {
            assertShows(
                    forStateUpdate(acting, taskState, /* isWarning= */ false),
                    NotificationState.STOPPED,
                    R.string.actor_notification_title_task_stopped,
                    R.string.actor_notification_body_stopped,
                    R.string.actor_notification_live_status_stopped,
                    /* isHighPriority= */ false,
                    /* isTerminal= */ true);
        }
    }

    @Test
    public void testForTask_StepProgressUpdatesBodyAndIsAlwaysSilent() {
        ActorNotificationConfig acting =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ false);
        ActorNotificationConfig step =
                forTask(
                        acting,
                        snapshot(ActorTaskState.ACTING, STEP, /* isExperimental= */ false),
                        Trigger.STEP_PROGRESS);

        assertEquals(NotificationState.WORKING, step.getState());
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_working_with_step_info, TASK_TITLE, STEP),
                step.getBody());
        assertTrue(step.isSilent());
        assertTrue(step.shouldRebuild(acting));

        ActorNotificationConfig emptyStep =
                forTask(
                        acting,
                        snapshot(ActorTaskState.ACTING, "", /* isExperimental= */ false),
                        Trigger.STEP_PROGRESS);
        assertEquals(
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE),
                emptyStep.getBody());
        assertTrue(emptyStep.isSilent());
    }

    @Test
    public void testWarningMode_WarningFollowsTaskState() {
        ActorNotificationConfig warningNoResponse =
                forStateUpdate(
                        /* previous= */ null,
                        ActorTaskState.WAITING_ON_USER,
                        /* isWarning= */ true);
        assertShows(
                warningNoResponse,
                NotificationState.WARNING_NO_RESPONSE,
                R.string.actor_notification_title_will_stop_task,
                R.string.actor_notification_body_will_stop_task_no_response,
                R.string.actor_notification_live_status_review,
                /* isHighPriority= */ true,
                /* isTerminal= */ false);
        assertTrue(warningNoResponse.isWarningMode());

        ActorNotificationConfig warningLongRunning =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ true);
        assertShows(
                warningLongRunning,
                NotificationState.WARNING_LONG_RUNNING,
                R.string.actor_notification_title_will_stop_task,
                R.string.actor_notification_body_will_stop_task_long_running,
                R.string.actor_notification_live_status_review,
                /* isHighPriority= */ true,
                /* isTerminal= */ false);
        assertTrue(warningLongRunning.isWarningMode());
    }

    @Test
    public void testWarningMode_StepProgressAndResendKeepWarningForLaterRefresh() {
        ActorNotificationConfig warning =
                forStateUpdate(
                        /* previous= */ null,
                        ActorTaskState.PAUSED_BY_ACTOR,
                        /* isWarning= */ true);
        ActorNotificationConfig resend =
                forTask(warning, snapshot(ActorTaskState.PAUSED_BY_ACTOR), Trigger.RESEND_LOUD);
        assertEquals(NotificationState.PAUSED, resend.getState());
        assertTrue(resend.isWarningMode());

        ActorNotificationConfig step =
                forTask(warning, snapshot(ActorTaskState.PAUSED_BY_ACTOR), Trigger.STEP_PROGRESS);
        assertEquals(NotificationState.PAUSED, step.getState());
        assertTrue(step.isWarningMode());

        ActorNotificationConfig refresh =
                forTask(step, snapshot(ActorTaskState.PAUSED_BY_ACTOR), Trigger.TIMEOUT_WARNING);

        assertFalse(refresh.shouldRebuild(step));
    }

    @Test
    public void testWarningMode_RepostShowsWarningAfterStepProgress() {
        ActorNotificationConfig warning =
                forStateUpdate(
                        /* previous= */ null,
                        ActorTaskState.WAITING_ON_USER,
                        /* isWarning= */ true);
        ActorNotificationConfig step =
                forTask(warning, snapshot(ActorTaskState.WAITING_ON_USER), Trigger.STEP_PROGRESS);
        assertEquals(NotificationState.WAITING_ON_USER, step.getState());

        ActorNotificationConfig reposted =
                forTask(step, snapshot(ActorTaskState.WAITING_ON_USER), Trigger.REPOST);
        assertEquals(NotificationState.WARNING_NO_RESPONSE, reposted.getState());
        assertTrue(reposted.isSilent());
    }

    @Test
    public void testWarningMode_ClearedByStateUpdateWithoutWarning() {
        ActorNotificationConfig warning =
                forStateUpdate(
                        /* previous= */ null,
                        ActorTaskState.PAUSED_BY_ACTOR,
                        /* isWarning= */ true);
        assertTrue(warning.isWarningMode());

        ActorNotificationConfig resumed =
                forStateUpdate(warning, ActorTaskState.PAUSED_BY_ACTOR, /* isWarning= */ false);

        assertFalse(resumed.isWarningMode());
        assertTrue(resumed.shouldRebuild(warning));
    }

    @Test
    public void testWarningMode_ClearedAfterStepProgressOrResend() {
        ActorNotificationConfig warning =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ true);
        for (@Trigger int trigger : new int[] {Trigger.STEP_PROGRESS, Trigger.RESEND_LOUD}) {
            ActorNotificationConfig plain =
                    forTask(warning, snapshot(ActorTaskState.ACTING), trigger);
            assertEquals("Trigger " + trigger, NotificationState.WORKING, plain.getState());
            assertTrue("Trigger " + trigger, plain.isWarningMode());

            ActorNotificationConfig left =
                    forStateUpdate(plain, ActorTaskState.ACTING, /* isWarning= */ false);

            assertFalse("Trigger " + trigger, left.isWarningMode());
            assertTrue("Trigger " + trigger, left.shouldRebuild(plain));
        }
    }

    @Test
    public void testIsSilent_StepProgressAndRepostAreAlwaysSilent() {
        ActorNotificationConfig acting =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ false);
        for (@Trigger int trigger : new int[] {Trigger.STEP_PROGRESS, Trigger.REPOST}) {
            ActorNotificationConfig config =
                    forTask(acting, snapshot(ActorTaskState.ACTING), trigger);
            assertTrue("Trigger " + trigger + " never alerts", config.isSilent());
        }
    }

    @Test
    public void testIsSilent_StateUpdatesFollowTabVisibility() {
        for (boolean isTabVisible : new boolean[] {false, true}) {
            ActorNotificationConfig config =
                    forTask(
                            /* previous= */ null,
                            snapshot(ActorTaskState.WAITING_ON_USER),
                            Trigger.TASK_STATE_CHANGED,
                            isTabVisible);
            assertEquals("Tab visible: " + isTabVisible, isTabVisible, config.isSilent());
        }
    }

    @Test
    public void testIsSilent_ResendIgnoresTabVisibility() {
        ActorNotificationConfig visibleActing =
                forTask(
                        /* previous= */ null,
                        snapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED,
                        /* isTabVisible= */ true);
        assertTrue(visibleActing.isSilent());

        ActorNotificationConfig resend =
                forTask(
                        visibleActing,
                        snapshot(ActorTaskState.ACTING),
                        Trigger.RESEND_LOUD,
                        /* isTabVisible= */ true);

        assertFalse(resend.isSilent());
    }

    @Test
    public void testIsSilent_ExperimentalSilenceAllowsOnlyFirstWorking() {
        ActorNotificationConfig created =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.CREATED),
                        Trigger.TASK_STATE_CHANGED);
        assertTrue(created.isSilent());

        ActorNotificationConfig acting =
                forTask(
                        created,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);
        assertFalse(acting.isSilent());

        ActorNotificationConfig paused =
                forTask(
                        acting,
                        experimentalSnapshot(ActorTaskState.PAUSED_BY_USER),
                        Trigger.TASK_STATE_CHANGED);
        assertTrue(paused.isSilent());

        ActorNotificationConfig actingAgain =
                forTask(
                        paused,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);
        assertTrue(actingAgain.isSilent());

        ActorNotificationConfig warning =
                forTask(
                        actingAgain,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TIMEOUT_WARNING);
        assertTrue(warning.isSilent());
    }

    @Test
    public void testIsHighPriority_UserAttentionStates() {
        assertTrue(
                forStateUpdate(
                                /* previous= */ null,
                                ActorTaskState.WAITING_ON_USER,
                                /* isWarning= */ false)
                        .isHighPriority());
        assertTrue(
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ true)
                        .isHighPriority());
        assertTrue(
                forStateUpdate(
                                /* previous= */ null,
                                ActorTaskState.WAITING_ON_USER,
                                /* isWarning= */ true)
                        .isHighPriority());
        for (@ActorTaskState
        int taskState :
                new int[] {
                    ActorTaskState.ACTING,
                    ActorTaskState.PAUSED_BY_USER,
                    ActorTaskState.FINISHED,
                    ActorTaskState.FAILED
                }) {
            assertFalse(
                    "Task state " + taskState,
                    forStateUpdate(/* previous= */ null, taskState, /* isWarning= */ false)
                            .isHighPriority());
        }
    }

    @Test
    public void testForTask_LiveUpdatesDisabledIsOnlyAlertOnceWithoutChip() {
        ActorNotificationConfig config =
                ActorNotificationConfig.forTask(
                        mContext,
                        /* previous= */ null,
                        snapshot(ActorTaskState.PAUSED_BY_USER),
                        Trigger.TASK_STATE_CHANGED,
                        /* isLive= */ true,
                        /* isLiveUpdatesEnabled= */ false,
                        /* isTabVisible= */ false);

        assertFalse(config.isOngoing());
        assertTrue(config.isOnlyAlertOnce());
        assertEquals(0, config.getChipRes());
    }

    @Test
    public void testForTask_NonLiveStateChangeAlertsWhileTabHidden() {
        ActorNotificationConfig config =
                ActorNotificationConfig.forTask(
                        mContext,
                        /* previous= */ null,
                        snapshot(ActorTaskState.WAITING_ON_USER),
                        Trigger.TASK_STATE_CHANGED,
                        /* isLive= */ false,
                        /* isLiveUpdatesEnabled= */ true,
                        /* isTabVisible= */ false);

        assertFalse(config.isOngoing());
        assertFalse(config.isSilent());
    }

    @Test
    public void testShouldRebuild_ResendLoudSkipsRebuildWhenSilent() {
        ActorNotificationConfig acting =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);
        assertFalse(acting.isSilent());

        ActorNotificationConfig resend =
                forTask(acting, experimentalSnapshot(ActorTaskState.ACTING), Trigger.RESEND_LOUD);

        assertTrue(resend.isSilent());
        assertFalse(resend.shouldRebuild(acting));

        ActorNotificationConfig resendWithoutNotification =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.CREATED),
                        Trigger.RESEND_LOUD);
        assertTrue(resendWithoutNotification.isSilent());
        assertFalse(resendWithoutNotification.shouldRebuild(/* previous= */ null));
    }

    @Test
    public void testShouldRebuild_AlwaysForUnconditionalPosts() {
        ActorNotificationConfig previous =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ false);
        for (@Trigger
        int trigger : new int[] {Trigger.STEP_PROGRESS, Trigger.RESEND_LOUD, Trigger.REPOST}) {
            ActorNotificationConfig current =
                    forTask(previous, snapshot(ActorTaskState.ACTING), trigger);
            assertTrue("Trigger " + trigger, current.shouldRebuild(previous));
        }
    }

    @Test
    public void testShouldRebuild_TransitionsBetweenDistinctCategories() {
        ActorNotificationConfig created =
                forStateUpdate(
                        /* previous= */ null, ActorTaskState.CREATED, /* isWarning= */ false);
        ActorNotificationConfig acting =
                forStateUpdate(created, ActorTaskState.ACTING, /* isWarning= */ false);
        assertTrue(acting.shouldRebuild(created));

        ActorNotificationConfig reflecting =
                forStateUpdate(acting, ActorTaskState.REFLECTING, /* isWarning= */ false);
        assertFalse(reflecting.shouldRebuild(acting));

        ActorNotificationConfig waiting =
                forStateUpdate(acting, ActorTaskState.WAITING_ON_USER, /* isWarning= */ false);
        assertTrue(waiting.shouldRebuild(acting));

        ActorNotificationConfig paused =
                forStateUpdate(waiting, ActorTaskState.PAUSED_BY_USER, /* isWarning= */ false);
        assertTrue(paused.shouldRebuild(waiting));

        ActorNotificationConfig pausedByActor =
                forStateUpdate(paused, ActorTaskState.PAUSED_BY_ACTOR, /* isWarning= */ false);
        assertFalse(pausedByActor.shouldRebuild(paused));

        ActorNotificationConfig completed =
                forStateUpdate(paused, ActorTaskState.FINISHED, /* isWarning= */ false);
        assertTrue(completed.shouldRebuild(paused));

        ActorNotificationConfig stopped =
                forStateUpdate(completed, ActorTaskState.CANCELLED, /* isWarning= */ false);
        assertTrue(stopped.shouldRebuild(completed));
    }

    @Test
    public void testShouldRebuild_MatchesServiceForEveryStateAndWarningPair() {
        for (@ActorTaskState int oldState : ALL_TASK_STATES) {
            for (@ActorTaskState int newState : ALL_TASK_STATES) {
                for (boolean wasWarning : new boolean[] {false, true}) {
                    for (boolean isWarning : new boolean[] {false, true}) {
                        ActorNotificationConfig previous =
                                forStateUpdate(/* previous= */ null, oldState, wasWarning);
                        ActorNotificationConfig current =
                                forStateUpdate(previous, newState, isWarning);
                        boolean expected =
                                isWarning != wasWarning
                                        || ActorNotificationFactory.shouldUpdateNotification(
                                                oldState, newState);
                        assertEquals(
                                "From "
                                        + oldState
                                        + (wasWarning ? " (warning)" : "")
                                        + " to "
                                        + newState
                                        + (isWarning ? " (warning)" : ""),
                                expected,
                                current.shouldRebuild(previous));
                    }
                }
            }
        }
    }

    @Test
    public void testShouldRebuild_CreatedToActingAlertsAgain() {
        ActorNotificationConfig created =
                forStateUpdate(
                        /* previous= */ null, ActorTaskState.CREATED, /* isWarning= */ false);
        ActorNotificationConfig acting =
                forStateUpdate(created, ActorTaskState.ACTING, /* isWarning= */ false);

        assertFalse(created.isSilent());
        assertTrue(acting.shouldRebuild(created));
        assertFalse(acting.isSilent());
        assertFalse(acting.isOnlyAlertOnce());
    }

    @Test
    public void testHasShownLoudWorking_SetByLoudResend() {
        ActorNotificationConfig visibleActing =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED,
                        /* isTabVisible= */ true);
        ActorNotificationConfig resend =
                forTask(
                        visibleActing,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.RESEND_LOUD,
                        /* isTabVisible= */ true);
        assertFalse(resend.isSilent());

        ActorNotificationConfig paused =
                forTask(
                        resend,
                        experimentalSnapshot(ActorTaskState.PAUSED_BY_USER),
                        Trigger.TASK_STATE_CHANGED);
        ActorNotificationConfig actingAgain =
                forTask(
                        paused,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);

        assertTrue(actingAgain.isSilent());
    }

    @Test
    public void testHasShownLoudWorking_NotSetWhenUpdateIsNotPosted() {
        ActorNotificationConfig visibleActing =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED,
                        /* isTabVisible= */ true);
        ActorNotificationConfig hiddenReflecting =
                forTask(
                        visibleActing,
                        experimentalSnapshot(ActorTaskState.REFLECTING),
                        Trigger.TASK_STATE_CHANGED);
        assertTrue(visibleActing.isSilent());
        assertFalse(hiddenReflecting.shouldRebuild(visibleActing));

        ActorNotificationConfig paused =
                forTask(
                        hiddenReflecting,
                        experimentalSnapshot(ActorTaskState.PAUSED_BY_USER),
                        Trigger.TASK_STATE_CHANGED);
        ActorNotificationConfig actingAgain =
                forTask(
                        paused,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);

        assertFalse(actingAgain.isSilent());
    }

    @Test
    public void testHasShownLoudWorking_ClearedWhenTaskCompletes() {
        ActorNotificationConfig acting =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);
        assertFalse(acting.isSilent());

        ActorNotificationConfig finished =
                forTask(
                        acting,
                        experimentalSnapshot(ActorTaskState.FINISHED),
                        Trigger.TASK_STATE_CHANGED);
        ActorNotificationConfig actingAgain =
                forTask(
                        finished,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);

        assertFalse(actingAgain.isSilent());
    }

    @Test
    public void testNotificationTag_CarriedForwardAndCleared() {
        ActorNotificationConfig untagged =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ false);
        ActorNotificationConfig tagged =
                untagged.withNotificationTag(ActorNotificationService.UNPINNED_NOTIFICATION_TAG);
        ActorNotificationConfig next =
                forStateUpdate(tagged, ActorTaskState.PAUSED_BY_USER, /* isWarning= */ false);
        assertEquals(ActorNotificationService.UNPINNED_NOTIFICATION_TAG, next.getNotificationTag());

        assertNull(next.withNotificationTag(null).getNotificationTag());
    }

    @Test
    public void testWithTaskStateFrom_KeepsNotificationAndTakesTaskState() {
        ActorNotificationConfig acting =
                forStateUpdate(/* previous= */ null, ActorTaskState.ACTING, /* isWarning= */ false);
        ActorNotificationConfig reflecting =
                acting.withTaskStateFrom(
                        forStateUpdate(acting, ActorTaskState.REFLECTING, /* isWarning= */ false));

        assertEquals(ActorTaskState.REFLECTING, reflecting.getTaskState());
        assertEquals(acting.getState(), reflecting.getState());
        assertEquals(acting.getBody(), reflecting.getBody());
        assertEquals(acting.isSilent(), reflecting.isSilent());
    }

    @Test
    public void testWithTaskStateFrom_TakesResolvedWarningMode() {
        ActorNotificationConfig warning =
                forStateUpdate(
                        /* previous= */ null,
                        ActorTaskState.PAUSED_BY_ACTOR,
                        /* isWarning= */ true);
        ActorNotificationConfig step =
                forTask(warning, snapshot(ActorTaskState.PAUSED_BY_ACTOR), Trigger.STEP_PROGRESS);
        ActorNotificationConfig left =
                forStateUpdate(step, ActorTaskState.PAUSED_BY_ACTOR, /* isWarning= */ false);

        ActorNotificationConfig kept = step.withTaskStateFrom(left);

        assertEquals(NotificationState.PAUSED, kept.getState());
        assertFalse(kept.isWarningMode());
        assertEquals(
                NotificationState.PAUSED,
                forTask(kept, snapshot(ActorTaskState.PAUSED_BY_ACTOR), Trigger.REPOST).getState());
    }

    @Test
    public void testWithTaskStateFrom_DropsLoudWorkingForCompletedTask() {
        ActorNotificationConfig acting =
                forTask(
                        /* previous= */ null,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);
        assertFalse(acting.isSilent());

        ActorNotificationConfig kept =
                acting.withTaskStateFrom(
                        forTask(
                                acting,
                                experimentalSnapshot(ActorTaskState.FINISHED),
                                Trigger.TASK_STATE_CHANGED));
        ActorNotificationConfig actingAgain =
                forTask(
                        kept,
                        experimentalSnapshot(ActorTaskState.ACTING),
                        Trigger.TASK_STATE_CHANGED);

        assertFalse(actingAgain.isSilent());
    }

    @Test
    public void testTaskSnapshotOf_CapturesTaskFacts() {
        when(mTask.getId()).thenReturn(TASK_ID);
        when(mTask.getTitle()).thenReturn(TASK_TITLE);
        when(mTask.getTargetTabId()).thenReturn(TAB_ID);
        when(mTask.getGlicConversationId()).thenReturn(CONVERSATION_ID);

        TaskSnapshot snapshot =
                TaskSnapshot.of(
                        mTask,
                        ActorTaskState.ACTING,
                        STEP,
                        /* isExperimentalTriggeringSilenceEnabled= */ true);
        ActorNotificationConfig config =
                forTask(/* previous= */ null, snapshot, Trigger.TASK_STATE_CHANGED);

        assertEquals(TASK_ID, config.getNotificationId());
        assertEquals(TAB_ID, config.getTargetTabId());
        assertEquals(CONVERSATION_ID, config.getGlicConversationId());
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_working_with_step_info, TASK_TITLE, STEP),
                config.getBody());
        assertEquals(TASK_TITLE, snapshot.title);
        assertEquals(ActorTaskState.ACTING, config.getTaskState());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testFactoryParity_LiveUpdatesEnabled() {
        when(mTask.getId()).thenReturn(TASK_ID);
        when(mTask.getTitle()).thenReturn(TASK_TITLE);
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask))
                .thenReturn(new Intent("DEFAULT_ACTION"));
        when(mTask.getCurrentActionName()).thenReturn(STEP);
        assertFactoryParity(STEP);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
    public void testFactoryParity_LiveUpdatesDisabled() {
        when(mTask.getId()).thenReturn(TASK_ID);
        when(mTask.getTitle()).thenReturn(TASK_TITLE);
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask))
                .thenReturn(new Intent("DEFAULT_ACTION"));
        assertFactoryParity(/* stepProgressText= */ null);
    }

    private void assertFactoryParity(@Nullable String stepProgressText) {
        boolean isLiveUpdatesEnabled = ActorUtils.isLiveNotificationEnabled();
        for (@ActorTaskState int state : ALL_TASK_STATES) {
            for (boolean isWarning : new boolean[] {false, true}) {
                for (boolean isLive : new boolean[] {false, true}) {
                    ActorNotificationConfig config =
                            ActorNotificationConfig.forTask(
                                    mContext,
                                    /* previous= */ null,
                                    snapshot(state, stepProgressText, /* isExperimental= */ false),
                                    isWarning
                                            ? Trigger.TIMEOUT_WARNING
                                            : Trigger.TASK_STATE_CHANGED,
                                    isLive,
                                    isLiveUpdatesEnabled,
                                    /* isTabVisible= */ false);
                    Notification notification =
                            ActorNotificationFactory.buildNotification(
                                            mTask, state, config.isSilent(), isWarning, isLive)
                                    .getNotification();
                    assertNotificationMatches(
                            "State " + state + ", warning " + isWarning + ", live " + isLive,
                            config,
                            notification);
                }
            }
        }
    }

    private void assertNotificationMatches(
            String message, ActorNotificationConfig config, Notification notification) {
        assertEquals(
                message,
                mContext.getString(config.getTitleRes()),
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        CharSequence text = notification.extras.getCharSequence(Notification.EXTRA_TEXT);
        assertEquals(message, config.getBody(), text == null ? null : text.toString());
        CharSequence chip =
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT);
        assertEquals(
                message,
                config.getChipRes() == 0 ? null : mContext.getString(config.getChipRes()),
                chip == null ? null : chip.toString());
        assertEquals(
                message,
                config.isOnlyAlertOnce(),
                (notification.flags & Notification.FLAG_ONLY_ALERT_ONCE) != 0);
        int expectedPriority =
                config.isHighPriority()
                        ? Notification.PRIORITY_HIGH
                        : Notification.PRIORITY_DEFAULT;
        assertEquals(message, expectedPriority, notification.priority);
        assertEquals(
                message,
                config.isTerminal(),
                (notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
        boolean expectedOngoingEvent = !config.isTerminal() || config.isOngoing();
        assertEquals(
                message,
                expectedOngoingEvent,
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertEquals(
                message,
                config.isOngoing(),
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
    }
}
