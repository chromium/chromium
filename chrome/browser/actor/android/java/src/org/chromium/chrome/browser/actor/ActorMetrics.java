// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.content.Intent;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.WeakHashMap;

/** Helper class for recording Actor-related UMA metrics. */
@NullMarked
public class ActorMetrics implements ActorKeyedService.Observer {
    public static final String ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY =
            "Actor.BackgroundActuation.OnTabAdded.Latency";
    public static final String ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY_COLD =
            "Actor.BackgroundActuation.OnTabAdded.Latency.ColdStart";
    public static final String ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY_WARM =
            "Actor.BackgroundActuation.OnTabAdded.Latency.WarmStart";
    public static final String ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES =
            "Actor.Notification.TimeBetweenWorklogUpdates";
    public static final String ACTOR_TASK_STOPPED_REASON_BACKGROUND_ACTUATION =
            "Actor.Task.StoppedReason.BackgroundActuation";
    public static final String ACTOR_TASK_STOPPED_REASON_FOREGROUND =
            "Actor.Task.StoppedReason.Foreground";
    public static final String ACTOR_TASK_STOPPED_REASON_PIP = "Actor.Task.StoppedReason.Pip";

    private static final int INVALID_TASK_ID = -1;
    private static final int INVALID_TASK_STATE = -1;
    private static final Set<Intent> sRecordedIntents =
            Collections.synchronizedSet(Collections.newSetFromMap(new WeakHashMap<>()));

    // LINT.IfChange(ActorPipStatus)

    @IntDef({ActorPipStatus.ENTERED, ActorPipStatus.EXITED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipStatus {
        int ENTERED = 0;
        int EXITED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorPipStatus)

    // LINT.IfChange(ActorPipExitReason)

    @IntDef({
        ActorPipExitReason.CLOSE,
        ActorPipExitReason.EXPAND,
        ActorPipExitReason.COMPLETED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipExitReason {
        int CLOSE = 0;
        int EXPAND = 1;
        int COMPLETED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorPipExitReason)

    // LINT.IfChange(ActorPipUserInteraction)

    @IntDef({
        ActorPipUserInteraction.PAUSE,
        ActorPipUserInteraction.RESUME,
        ActorPipUserInteraction.EXPAND
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipUserInteraction {
        int PAUSE = 0;
        int RESUME = 1;
        int EXPAND = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorPipUserInteraction)

    // LINT.IfChange(ActorNotificationPermissionState)

    @IntDef({
        ActorNotificationPermissionState.ENABLED,
        ActorNotificationPermissionState.CHANNEL_DISABLED,
        ActorNotificationPermissionState.OS_DISABLED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorNotificationPermissionState {
        int ENABLED = 0;
        int CHANNEL_DISABLED = 1;
        int OS_DISABLED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorNotificationPermissionState)

    @IntDef({
        ActorPauseResumeSource.PIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPauseResumeSource {
        int PIP = 0;
    }

    /** Represents whether Chrome is in Picture-in-Picture mode or standard Foreground mode. */
    @IntDef({ActorMode.FOREGROUND, ActorMode.PIP})
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE, ElementType.PARAMETER, ElementType.FIELD})
    public @interface ActorMode {
        int FOREGROUND = 0;
        int PIP = 1;
        int NUM_ENTRIES = 2;
    }

    private static @Nullable ActorMetrics sInstance;

    private final Map<@ActorTaskId Integer, LatencyTracker> mTrackers = new HashMap<>();
    private final Map<@ActorTaskId Integer, Integer> mOmniboxClickCounts = new HashMap<>();
    private final Set<@ActorTaskId Integer> mStoppedTasks = new HashSet<>();
    private final Set<@ActorTaskId Integer> mRecordedBackgroundActuationTaskIds = new HashSet<>();
    private @ActorMode int mCurrentGlobalMode = ActorMode.FOREGROUND;

    /**
     * Retrieves the global ActorMetrics instance.
     *
     * @return The ActorMetrics instance.
     */
    public static ActorMetrics getInstance() {
        if (sInstance == null) {
            sInstance = new ActorMetrics();
        }
        return sInstance;
    }

    // LINT.IfChange(ActorBackgroundActuationTrigger)

    @IntDef({
        ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME,
        ActorBackgroundActuationTrigger.TASK_INIT_BY_FCM,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorBackgroundActuationTrigger {
        int TASK_INIT_BY_CHROME = 0;
        int TASK_INIT_BY_FCM = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorBackgroundActuationTrigger)

    private ActorMetrics() {}

    /**
     * Records omnibox focus/click metrics if the given tab has an active Actor task bound to it.
     *
     * @param tab The current {@link Tab}.
     */
    public static void recordOmniboxFocus(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        Profile profile = tab.getProfile();
        if (profile == null) return;
        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
        if (service == null || service.getActiveTasksCount() == 0) return;

        @Nullable
        @ActorTaskId
        Integer taskId = service.getActiveTaskIdOnTab(tab.getId(), /* includePaused= */ true);
        if (taskId == null) return;

        ActorTask task = service.getTask(taskId);
        if (task == null) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Ui.OmniboxClick.TaskState", task.getState(), ActorTaskState.MAX_VALUE + 1);
        getInstance().incrementOmniboxClickCount(taskId);
    }

    private void incrementOmniboxClickCount(@ActorTaskId int taskId) {
        mOmniboxClickCounts.put(taskId, mOmniboxClickCounts.getOrDefault(taskId, 0) + 1);
    }

    @Override
    public void onTaskStopped(@ActorTaskId int taskId, @StoppedReason int stoppedReason) {
        if (!mStoppedTasks.add(taskId)) {
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                getStoppedReasonModeHistogram(taskId), stoppedReason, StoppedReason.MAX_VALUE + 1);

        String reasonName = getStoppedReasonName(stoppedReason);
        if (!reasonName.isEmpty()) {
            int clickCount = mOmniboxClickCounts.getOrDefault(taskId, 0);
            RecordHistogram.recordCount100Histogram(
                    "Actor.Task.OmniboxClickCount." + reasonName, clickCount);
        }
        mOmniboxClickCounts.remove(taskId);
    }

    /**
     * Returns the stopped reason histogram for the task's execution mode. Relies on ActorMetrics
     * being registered before ActorForegroundServiceManager so the background session is checked
     * before cleanup. Background actuation takes precedence over PiP.
     */
    private String getStoppedReasonModeHistogram(@ActorTaskId int taskId) {
        if (ActorForegroundServiceController.get().hasBackgroundSessionForTask(taskId)) {
            return ACTOR_TASK_STOPPED_REASON_BACKGROUND_ACTUATION;
        }
        if (mCurrentGlobalMode == ActorMode.PIP) {
            return ACTOR_TASK_STOPPED_REASON_PIP;
        }
        return ACTOR_TASK_STOPPED_REASON_FOREGROUND;
    }

    /**
     * Records the elapsed time in milliseconds between consecutive worklog updates on an active
     * Actor task notification.
     */
    public static void recordTimeBetweenWorklogUpdates(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES, durationMs);
    }

    /**
     * Records the latency in milliseconds while waiting for a background tab to be restored and
     * added to the {@link org.chromium.chrome.browser.tabmodel.TabModel}.
     *
     * @param durationMs The elapsed latency in milliseconds.
     * @param isColdStart Whether the {@link org.chromium.chrome.browser.tabmodel.TabModelSelector}
     *     tab state was uninitialized when waiting began.
     */
    public static void recordOnTabAddedLatency(long durationMs, boolean isColdStart) {
        RecordHistogram.recordMediumTimesHistogram(
                ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY, durationMs);
        RecordHistogram.recordMediumTimesHistogram(
                isColdStart
                        ? ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY_COLD
                        : ACTOR_BACKGROUND_ACTUATION_ON_TAB_ADDED_LATENCY_WARM,
                durationMs);
    }

    /** Records the trigger source that initiated background actuation. */
    public static void recordBackgroundActuationTrigger(
            @ActorBackgroundActuationTrigger int trigger) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.BackgroundActuation.Trigger",
                trigger,
                ActorBackgroundActuationTrigger.NUM_ENTRIES);
    }

    /**
     * Records the trigger source that initiated background actuation for a task if not already
     * recorded for that task.
     *
     * @param taskId The task ID to check and record for.
     * @param trigger The trigger source.
     */
    public static void maybeRecordBackgroundActuationTrigger(
            @ActorTaskId int taskId, @ActorBackgroundActuationTrigger int trigger) {
        ThreadUtils.assertOnUiThread();
        if (!getInstance().mRecordedBackgroundActuationTaskIds.add(taskId)) {
            return;
        }
        recordBackgroundActuationTrigger(trigger);
    }

    /**
     * Marks a task as having already had its background actuation trigger recorded (e.g. when the
     * task was originally initiated via FCM) so subsequent foreground-to-background transitions do
     * not record {@link ActorBackgroundActuationTrigger#TASK_INIT_BY_CHROME}.
     *
     * @param taskId The task ID to mark as recorded.
     */
    public static void markBackgroundActuationTriggerRecorded(@ActorTaskId int taskId) {
        ThreadUtils.assertOnUiThread();
        getInstance().mRecordedBackgroundActuationTaskIds.add(taskId);
    }

    /** Records the PiP status (Enter/Exit). */
    public static void recordPipStatus(@ActorPipStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.Status", status, ActorPipStatus.NUM_ENTRIES);
    }

    /** Records the PiP exit reason. */
    public static void recordPipExitReason(@ActorPipExitReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.ExitReason", reason, ActorPipExitReason.NUM_ENTRIES);
    }

    /** Records the PiP duration. */
    public static void recordPipDuration(long durationMs) {
        RecordHistogram.recordLongTimesHistogram("Actor.Pip.Duration", durationMs);
    }

    /** Records an interaction with the PiP window. */
    public static void recordPipUserInteraction(@ActorPipUserInteraction int interaction) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.UserInteractions", interaction, ActorPipUserInteraction.NUM_ENTRIES);
    }

    /** Records the notification permission state when an Actor task starts. */
    public static void recordNotificationPermissionState(
            @ActorNotificationPermissionState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Notification.PermissionState",
                state,
                ActorNotificationPermissionState.NUM_ENTRIES);
    }

    /**
     * Records ActorTaskState metrics from active task or intent.
     *
     * @param intent The intent to inspect.
     * @param profile The current Profile.
     */
    public static void maybeRecordMetricsFromIntent(
            @Nullable Intent intent, @Nullable Profile profile) {
        if (intent == null) return;
        if (!intent.hasExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID)) return;
        if (sRecordedIntents.contains(intent)) return;

        int taskId = intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, INVALID_TASK_ID);
        if (taskId == INVALID_TASK_ID) return;

        int state = INVALID_TASK_STATE;
        if (profile != null) {
            // Prioritize live task state from ActorKeyedService.
            state = getActorTaskStateFromTaskId(taskId, profile);
        }

        // Fallback to the state extra if the task is no longer in memory.
        if (state == INVALID_TASK_STATE) {
            state =
                    intent.getIntExtra(
                            NotificationConstants.EXTRA_ACTOR_TASK_STATE, INVALID_TASK_STATE);
        }

        if (state != INVALID_TASK_STATE) {
            sRecordedIntents.add(intent);
            RecordHistogram.recordEnumeratedHistogram(
                    "Actor.Notification.ClickTaskState", state, ActorTaskState.MAX_VALUE + 1);
        }
    }

    private static int getActorTaskStateFromTaskId(int taskId, Profile profile) {
        int state = INVALID_TASK_STATE;
        // Prioritize live task state from the service.
        ActorKeyedService service =
                ActorKeyedServiceFactory.getForProfile(profile.getOriginalProfile());
        if (service != null) {
            ActorTask task = service.getTask(taskId);
            if (task != null) {
                state = task.getState();
            }
        }
        return state;
    }

    /**
     * Records the cumulative execution duration for a given task state and mode.
     *
     * @param state The {@link ActorTaskState} of the task.
     * @param mode The {@link ActorMode} (Foreground or Pip).
     * @param durationMs The cumulative duration in milliseconds.
     */
    public static void recordExecutionDuration(
            @ActorTaskState int state, @ActorMode int mode, long durationMs) {
        String stateName = getStateName(state);
        if (stateName.isEmpty()) return;

        String modeName = (mode == ActorMode.PIP) ? "Pip" : "Foreground";
        String histogramName = "Actor.Tools.ExecutionDuration." + modeName + "." + stateName;
        RecordHistogram.recordLongTimesHistogram(histogramName, durationMs);
    }

    private static String getStateName(@ActorTaskState int state) {
        switch (state) {
            case ActorTaskState.CREATED:
                return "Created";
            case ActorTaskState.ACTING:
                return "Acting";
            case ActorTaskState.REFLECTING:
                return "Reflecting";
            case ActorTaskState.PAUSED_BY_ACTOR:
                return "PausedByActor";
            case ActorTaskState.PAUSED_BY_USER:
                return "PausedByUser";
            case ActorTaskState.CANCELLED:
                return "Cancelled";
            case ActorTaskState.FINISHED:
                return "Finished";
            case ActorTaskState.WAITING_ON_USER:
                return "WaitingOnUser";
            case ActorTaskState.FAILED:
                return "Failed";
            default:
                return "";
        }
    }

    /**
     * Updates the global PiP mode status and flushes current durations for all active tasks.
     *
     * @param inPip Whether Chrome is in PiP mode.
     */
    public void setIsInPip(boolean inPip) {
        @ActorMode int newMode = inPip ? ActorMode.PIP : ActorMode.FOREGROUND;
        if (mCurrentGlobalMode == newMode) return;

        mCurrentGlobalMode = newMode;
        for (LatencyTracker tracker : mTrackers.values()) {
            tracker.updateTaskStateAndMode(tracker.getCurrentState(), mCurrentGlobalMode);
        }
    }

    @Override
    public void onTaskStateChanged(@ActorTaskId int taskId, @ActorTaskState int newState) {
        LatencyTracker tracker = mTrackers.get(taskId);
        if (tracker == null) {
            if (!ActorUtils.isCompletedState(newState)) {
                recordNotificationPermissionState(ActorUtils.getActorNotificationPermissionState());
            }
            tracker = new LatencyTracker(newState, mCurrentGlobalMode);
            mTrackers.put(taskId, tracker);
        } else {
            tracker.updateTaskStateAndMode(newState, mCurrentGlobalMode);
        }

        if (ActorUtils.isCompletedState(newState)) {
            tracker.recordTaskMetrics();
            mTrackers.remove(taskId);
            mRecordedBackgroundActuationTaskIds.remove(taskId);
            if (!mStoppedTasks.contains(taskId)) {
                @StoppedReason
                int defaultReason =
                        (newState == ActorTaskState.FINISHED)
                                ? StoppedReason.TASK_COMPLETE
                                : StoppedReason.STOPPED_BY_USER;
                onTaskStopped(taskId, defaultReason);
            }
        }
    }

    /** Stores cumulative latency metrics for a single task. */
    private static class LatencyTracker {
        private final Map<Pair<@ActorTaskState Integer, @ActorMode Integer>, Long>
                mAccumulatedDurations = new HashMap<>();
        private @ActorTaskState int mCurrentState;
        private @ActorMode int mCurrentMode;
        private long mLastTransitionTime;

        LatencyTracker(@ActorTaskState int initialState, @ActorMode int initialMode) {
            mCurrentState = initialState;
            mCurrentMode = initialMode;
            mLastTransitionTime = SystemClock.elapsedRealtime();
        }

        @ActorTaskState
        int getCurrentState() {
            return mCurrentState;
        }

        /**
         * Records the time elapsed in the current state/mode and transitions to the new ones.
         *
         * @param newState The next {@link ActorTaskState}.
         * @param newMode The next {@link ActorMode}.
         */
        void updateTaskStateAndMode(@ActorTaskState int newState, @ActorMode int newMode) {
            long now = SystemClock.elapsedRealtime();
            long elapsed = now - mLastTransitionTime;

            Pair<@ActorTaskState Integer, @ActorMode Integer> key =
                    new Pair<>(mCurrentState, mCurrentMode);
            long total = mAccumulatedDurations.getOrDefault(key, 0L);
            mAccumulatedDurations.put(key, total + elapsed);

            mCurrentState = newState;
            mCurrentMode = newMode;
            mLastTransitionTime = now;
        }

        /** Records all accumulated state/mode durations for this task to UMA histograms. */
        void recordTaskMetrics() {
            for (Map.Entry<Pair<@ActorTaskState Integer, @ActorMode Integer>, Long> entry :
                    mAccumulatedDurations.entrySet()) {
                @ActorTaskState int state = entry.getKey().first;
                @ActorMode int mode = entry.getKey().second;
                long duration = entry.getValue();
                if (duration > 0) {
                    recordExecutionDuration(state, mode, duration);
                }
            }
        }
    }

    public static void resetForTesting() {
        sInstance = null;
        sRecordedIntents.clear();
    }

    public void onTaskStateChangedForTesting(
            @ActorTaskId int taskId, @ActorTaskState int newState) {
        onTaskStateChanged(taskId, newState);
    }

    public void onTaskStoppedForTesting(@ActorTaskId int taskId, @StoppedReason int stoppedReason) {
        onTaskStopped(taskId, stoppedReason);
    }

    public int getOmniboxClickCountForTesting(@ActorTaskId int taskId) {
        return mOmniboxClickCounts.getOrDefault(taskId, 0);
    }

    // LINT.IfChange(StoppedReasonName)
    private static String getStoppedReasonName(@StoppedReason int stoppedReason) {
        switch (stoppedReason) {
            case StoppedReason.STOPPED_BY_USER:
                return "Cancelled";
            case StoppedReason.TASK_COMPLETE:
                return "Completed";
            case StoppedReason.MODEL_ERROR:
                return "ModelError";
            case StoppedReason.CHROME_FAILURE:
                return "ChromeFailure";
            case StoppedReason.TAB_DETACHED:
                return "TabDetached";
            case StoppedReason.SHUTDOWN:
                return "Shutdown";
            case StoppedReason.USER_STARTED_NEW_CHAT:
                return "NewChat";
            case StoppedReason.USER_LOADED_PREVIOUS_CHAT:
                return "PreviousChat";
            case StoppedReason.USER_NAVIGATED_AWAY:
                return "UserNavigatedAway";
            case StoppedReason.TIMEOUT:
                return "Timeout";
            default:
                return "";
        }
    }
    // LINT.ThenChange(//chrome/browser/actor/actor_task.h:StoppedReason,
    // //tools/metrics/histograms/metadata/actor/histograms.xml:StoppedReason)
}
