// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.notifications;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Immutable snapshot of an Actor task notification: the task facts it was resolved from and every
 * property {@link ActorNotificationFactory} reads to build it. Feature flags, Live Update support
 * and the {@link Context} are passed in, so resolving a config never reads global state.
 */
@NullMarked
final class ActorNotificationConfig {
    /** What the notification communicates to the user. */
    @IntDef({
        NotificationState.CREATED_PENDING_TAB,
        NotificationState.WORKING,
        NotificationState.PAUSED,
        NotificationState.WAITING_ON_USER,
        NotificationState.WARNING_LONG_RUNNING,
        NotificationState.WARNING_NO_RESPONSE,
        NotificationState.COMPLETED,
        NotificationState.STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface NotificationState {
        // Covers every ActorTaskState.CREATED task; a tab is not required.
        int CREATED_PENDING_TAB = 0;
        int WORKING = 1;
        int PAUSED = 2;
        int WAITING_ON_USER = 3;
        int WARNING_LONG_RUNNING = 4;
        int WARNING_NO_RESPONSE = 5;
        int COMPLETED = 6;
        int STOPPED = 7;
    }

    /** The event that caused a notification to be resolved again. */
    @IntDef({
        Trigger.TIMEOUT_WARNING,
        Trigger.STEP_PROGRESS,
        Trigger.RESEND_LOUD,
        Trigger.REPOST,
        Trigger.TASK_STATE_CHANGED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Trigger {
        // A task state change or visibility refresh while the task is in timeout warning mode.
        int TIMEOUT_WARNING = 0;
        int STEP_PROGRESS = 1;
        int RESEND_LOUD = 2;
        // Re-posts the notification silently, after the single live slot moves or when the
        // notification is unpinned.
        int REPOST = 3;
        // A task state change, or a refresh when the foreground service pins the notification.
        int TASK_STATE_CHANGED = 4;
    }

    /** The task facts a notification is resolved from, kept so it outlives the native task. */
    static final class TaskSnapshot {
        public final int taskId;
        public final String title;
        public final @Nullable String stepProgressText;
        public final int targetTabId;
        public final @Nullable String glicConversationId;
        public final @ActorTaskState int taskState;
        // Whether the task was experimentally triggered while
        // sActorSilenceNotificationsAfterFirstWorking is enabled.
        public final boolean isExperimentalTriggeringSilenceEnabled;

        TaskSnapshot(
                int taskId,
                String title,
                @Nullable String stepProgressText,
                int targetTabId,
                @Nullable String glicConversationId,
                @ActorTaskState int taskState,
                boolean isExperimentalTriggeringSilenceEnabled) {
            this.taskId = taskId;
            this.title = title;
            this.stepProgressText = stepProgressText;
            this.targetTabId = targetTabId;
            this.glicConversationId = glicConversationId;
            this.taskState = taskState;
            this.isExperimentalTriggeringSilenceEnabled = isExperimentalTriggeringSilenceEnabled;
        }

        /**
         * Captures the current facts of {@code task}.
         *
         * @param taskState The state to report, which may be the last observed state rather than
         *     {@code task.getState()}.
         * @param stepProgressText The current step, or null when step progress notifications are
         *     disabled.
         * @param isExperimentalTriggeringSilenceEnabled Whether experimental triggering silence
         *     applies to the task.
         */
        static TaskSnapshot of(
                ActorTask task,
                @ActorTaskState int taskState,
                @Nullable String stepProgressText,
                boolean isExperimentalTriggeringSilenceEnabled) {
            return new TaskSnapshot(
                    task.getId(),
                    task.getTitle(),
                    stepProgressText,
                    task.getTargetTabId(),
                    task.getGlicConversationId(),
                    taskState,
                    isExperimentalTriggeringSilenceEnabled);
        }
    }

    private final TaskSnapshot mTask;
    private final @Trigger int mTrigger;
    private final @NotificationState int mState;
    private final String mBody;
    private final boolean mIsOngoing;
    private final boolean mIsSilent;
    private final boolean mIsOnlyAlertOnce;
    // Timeout warning mode, which outlives notifications that do not show the warning.
    private final boolean mIsWarningMode;
    private final @Nullable String mNotificationTag;
    private final boolean mHasShownLoudWorking;

    private ActorNotificationConfig(
            TaskSnapshot task,
            @Trigger int trigger,
            @NotificationState int state,
            String body,
            boolean isOngoing,
            boolean isSilent,
            boolean isOnlyAlertOnce,
            boolean isWarningMode,
            @Nullable String notificationTag,
            boolean hasShownLoudWorking) {
        mTask = task;
        mTrigger = trigger;
        mState = state;
        mBody = body;
        mIsOngoing = isOngoing;
        mIsSilent = isSilent;
        mIsOnlyAlertOnce = isOnlyAlertOnce;
        mIsWarningMode = isWarningMode;
        mNotificationTag = notificationTag;
        mHasShownLoudWorking = hasShownLoudWorking;
    }

    /**
     * Resolves the next notification for a task.
     *
     * @param context The context used to resolve the body text.
     * @param previous The config of the task's current notification, or null if the task has no
     *     notification or no observed state yet.
     * @param task The task facts to show.
     * @param trigger The event being handled. State updates and visibility refreshes use {@link
     *     Trigger#TIMEOUT_WARNING} while the task is in timeout warning mode.
     * @param isLive Whether the task holds the single live (promoted ongoing) slot.
     * @param isLiveUpdatesEnabled Whether {@link ActorUtils#isLiveNotificationEnabled()} is true.
     * @param isTabVisible Whether one of the task's tabs is visible to the user.
     */
    static ActorNotificationConfig forTask(
            Context context,
            @Nullable ActorNotificationConfig previous,
            TaskSnapshot task,
            @Trigger int trigger,
            boolean isLive,
            boolean isLiveUpdatesEnabled,
            boolean isTabVisible) {
        boolean isWarningMode =
                switch (trigger) {
                    case Trigger.TIMEOUT_WARNING -> true;
                    case Trigger.TASK_STATE_CHANGED -> false;
                    default -> previous != null && previous.mIsWarningMode;
                };
        // Step progress and loud resends show the plain task state, even in warning mode.
        @NotificationState
        int state =
                isWarningMode && trigger != Trigger.STEP_PROGRESS && trigger != Trigger.RESEND_LOUD
                        ? warningStateForTask(task.taskState)
                        : stateForTask(task.taskState);
        boolean previousHasShownLoudWorking =
                !ActorUtils.isCompletedState(task.taskState)
                        && previous != null
                        && previous.mHasShownLoudWorking;
        boolean isSilent =
                isSilent(
                        state,
                        trigger,
                        isTabVisible,
                        task.isExperimentalTriggeringSilenceEnabled,
                        previousHasShownLoudWorking);
        boolean isOngoing = isLive && isLiveUpdatesEnabled;
        ActorNotificationConfig config =
                new ActorNotificationConfig(
                        task,
                        trigger,
                        state,
                        getBody(context, state, task),
                        isOngoing,
                        isSilent,
                        /* isOnlyAlertOnce= */ !isOngoing,
                        isWarningMode,
                        previous != null ? previous.mNotificationTag : null,
                        previousHasShownLoudWorking);
        boolean showsLoudWorking =
                task.isExperimentalTriggeringSilenceEnabled
                        && !isSilent
                        && state == NotificationState.WORKING
                        && config.shouldRebuild(previous);
        return showsLoudWorking ? config.withHasShownLoudWorking() : config;
    }

    // The warning follows the current task state, since the timeout that triggered it depends on
    // whether the task was waiting on the user.
    private static @NotificationState int warningStateForTask(@ActorTaskState int taskState) {
        return taskState == ActorTaskState.WAITING_ON_USER
                ? NotificationState.WARNING_NO_RESPONSE
                : NotificationState.WARNING_LONG_RUNNING;
    }

    private static @NotificationState int stateForTask(@ActorTaskState int taskState) {
        return switch (taskState) {
            case ActorTaskState.CREATED -> NotificationState.CREATED_PENDING_TAB;
            case ActorTaskState.ACTING, ActorTaskState.REFLECTING -> NotificationState.WORKING;
            case ActorTaskState.PAUSED_BY_USER, ActorTaskState.PAUSED_BY_ACTOR ->
                    NotificationState.PAUSED;
            case ActorTaskState.WAITING_ON_USER -> NotificationState.WAITING_ON_USER;
            case ActorTaskState.FINISHED -> NotificationState.COMPLETED;
            default -> NotificationState.STOPPED;
        };
    }

    private static @StringRes int getTitle(@NotificationState int state) {
        return switch (state) {
            case NotificationState.CREATED_PENDING_TAB, NotificationState.WORKING ->
                    R.string.actor_notification_title_working_on_task;
            case NotificationState.PAUSED -> R.string.actor_notification_title_task_paused;
            case NotificationState.WAITING_ON_USER ->
                    R.string.actor_notification_title_check_your_task;
            case NotificationState.WARNING_LONG_RUNNING, NotificationState.WARNING_NO_RESPONSE ->
                    R.string.actor_notification_title_will_stop_task;
            case NotificationState.COMPLETED -> R.string.actor_notification_title_task_complete;
            default -> R.string.actor_notification_title_task_stopped;
        };
    }

    private static String getBody(
            Context context, @NotificationState int state, TaskSnapshot task) {
        String title = task.title;
        @Nullable String step = task.stepProgressText;
        return switch (state) {
            case NotificationState.CREATED_PENDING_TAB, NotificationState.WORKING ->
                    (step != null && !step.isEmpty())
                            ? context.getString(
                                    R.string.actor_notification_body_working_with_step_info,
                                    title,
                                    step)
                            : context.getString(R.string.actor_notification_body_working, title);
            case NotificationState.PAUSED ->
                    context.getString(R.string.actor_notification_body_paused, title);
            case NotificationState.WAITING_ON_USER ->
                    context.getString(R.string.actor_notification_body_user_input, title);
            case NotificationState.WARNING_LONG_RUNNING ->
                    context.getString(
                            R.string.actor_notification_body_will_stop_task_long_running, title);
            case NotificationState.WARNING_NO_RESPONSE ->
                    context.getString(
                            R.string.actor_notification_body_will_stop_task_no_response, title);
            case NotificationState.COMPLETED ->
                    context.getString(R.string.actor_notification_body_complete, title);
            default -> context.getString(R.string.actor_notification_body_stopped, title);
        };
    }

    private static @StringRes int getChip(@NotificationState int state, boolean isOngoing) {
        if (!isOngoing) return 0;
        return switch (state) {
            case NotificationState.PAUSED -> R.string.actor_notification_live_status_paused;
            case NotificationState.WAITING_ON_USER,
                    NotificationState.WARNING_LONG_RUNNING,
                    NotificationState.WARNING_NO_RESPONSE ->
                    R.string.actor_notification_live_status_review;
            case NotificationState.COMPLETED -> R.string.actor_notification_live_status_done;
            case NotificationState.STOPPED -> R.string.actor_notification_live_status_stopped;
            default -> 0;
        };
    }

    private static boolean isSilent(
            @NotificationState int state,
            @Trigger int trigger,
            boolean isTabVisible,
            boolean isExperimentalTriggeringSilenceEnabled,
            boolean hasShownLoudWorking) {
        boolean isForcedSilent =
                isExperimentalTriggeringSilenceEnabled
                        && (state != NotificationState.WORKING || hasShownLoudWorking);
        return switch (trigger) {
            case Trigger.STEP_PROGRESS, Trigger.REPOST -> true;
            case Trigger.RESEND_LOUD -> isForcedSilent;
            default -> isTabVisible || isForcedSilent;
        };
    }

    /**
     * Returns whether this config must be built and posted to replace {@code previous}. Callers
     * skip rebuilding and posting when this returns false, updating only the cached task facts.
     */
    boolean shouldRebuild(@Nullable ActorNotificationConfig previous) {
        if (mTrigger == Trigger.RESEND_LOUD && mIsSilent) {
            return false;
        }
        if (previous == null) {
            return true;
        }
        return switch (mTrigger) {
            case Trigger.STEP_PROGRESS, Trigger.RESEND_LOUD, Trigger.REPOST -> true;
            // TODO(crbug.com/564644170): Compare resolved fields instead of task state categories
            // once same-state rebuilds post with onlyAlertOnce.
            default ->
                    mIsWarningMode != previous.mIsWarningMode
                            || ActorNotificationFactory.shouldUpdateNotification(
                                    previous.getTaskState(), getTaskState());
        };
    }

    /**
     * Returns a copy that keeps showing this config's notification but takes the task facts,
     * warning mode and loud working state from {@code resolved}. Callers use it when {@code
     * resolved} does not need a rebuild, so the kept config never carries state that {@code
     * resolved} dropped.
     */
    ActorNotificationConfig withTaskStateFrom(ActorNotificationConfig resolved) {
        return copy(
                resolved.mTask,
                resolved.mIsWarningMode,
                mNotificationTag,
                resolved.mHasShownLoudWorking);
    }

    /** Returns a copy posted under {@code notificationTag}. */
    ActorNotificationConfig withNotificationTag(@Nullable String notificationTag) {
        return copy(mTask, mIsWarningMode, notificationTag, mHasShownLoudWorking);
    }

    private ActorNotificationConfig withHasShownLoudWorking() {
        return copy(mTask, mIsWarningMode, mNotificationTag, /* hasShownLoudWorking= */ true);
    }

    private ActorNotificationConfig copy(
            TaskSnapshot task,
            boolean isWarningMode,
            @Nullable String notificationTag,
            boolean hasShownLoudWorking) {
        return new ActorNotificationConfig(
                task,
                mTrigger,
                mState,
                mBody,
                mIsOngoing,
                mIsSilent,
                mIsOnlyAlertOnce,
                isWarningMode,
                notificationTag,
                hasShownLoudWorking);
    }

    /** Returns the ID to post the notification under, which is the task ID. */
    int getNotificationId() {
        return mTask.taskId;
    }

    /** Returns the state the notification shows, which may be a warning state in warning mode. */
    @NotificationState
    int getState() {
        return mState;
    }

    /** Returns the task state the config was resolved from. */
    @ActorTaskState
    int getTaskState() {
        return mTask.taskState;
    }

    /**
     * Returns whether the task is in timeout warning mode, which step progress, resends and reposts
     * keep. ActorTaskTimeoutManager drives {@link Trigger#TIMEOUT_WARNING} through state updates,
     * so only reposts read this, to tell whether the warning reported for a repost changed.
     */
    boolean isWarningMode() {
        return mIsWarningMode;
    }

    @StringRes
    int getTitleRes() {
        return getTitle(mState);
    }

    String getBody() {
        return mBody;
    }

    /** Returns the Live Update status chip text, or 0 for an icon-only chip or no chip. */
    @StringRes
    int getChipRes() {
        return getChip(mState, mIsOngoing);
    }

    /**
     * Returns whether the notification is the task's ongoing Live Update. Callers request {@link
     * ActorNotificationFactory#EXTRA_REQUEST_PROMOTED_ONGOING} when true, and terminal
     * notifications use it for {@link android.app.Notification#FLAG_ONGOING_EVENT}. Non-terminal
     * task notifications are always ongoing events.
     */
    boolean isOngoing() {
        return mIsOngoing;
    }

    /**
     * Returns whether the notification must not alert. Callers suppress alerting when they post a
     * rebuilt notification, so this only applies when {@link #shouldRebuild} is true.
     */
    boolean isSilent() {
        return mIsSilent;
    }

    /**
     * Returns whether in-place updates of the notification must not alert again. Callers set {@link
     * android.app.Notification#FLAG_ONLY_ALERT_ONCE} when true. Notifications that are not ongoing
     * Live Updates are alert-once; live notifications are not.
     */
    boolean isOnlyAlertOnce() {
        return mIsOnlyAlertOnce;
    }

    /**
     * Returns whether the notification asks for the user's attention, such as waiting on input or
     * an upcoming timeout. Callers post it with {@link android.app.Notification#PRIORITY_HIGH}.
     */
    boolean isHighPriority() {
        return switch (mState) {
            case NotificationState.WAITING_ON_USER,
                    NotificationState.WARNING_LONG_RUNNING,
                    NotificationState.WARNING_NO_RESPONSE ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns whether the task completed or stopped. Callers set {@link
     * android.app.Notification#FLAG_AUTO_CANCEL} when true.
     */
    boolean isTerminal() {
        return mState == NotificationState.COMPLETED || mState == NotificationState.STOPPED;
    }

    /** The tag to post the notification under, or null for the default tag. */
    @Nullable String getNotificationTag() {
        return mNotificationTag;
    }

    /** Returns the tab the notification's routing intent brings to the front. */
    int getTargetTabId() {
        return mTask.targetTabId;
    }

    /** Returns the Glic conversation the routing intent opens, or null if there is none. */
    @Nullable String getGlicConversationId() {
        return mTask.glicConversationId;
    }
}
