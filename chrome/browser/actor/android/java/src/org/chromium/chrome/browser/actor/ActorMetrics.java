// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.content.Intent;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashMap;
import java.util.Map;

/** Helper class for recording Actor-related UMA metrics. */
@NullMarked
public class ActorMetrics implements ActorKeyedService.Observer {
    private static final int INVALID_TASK_ID = -1;
    private static final int INVALID_TASK_STATE = -1;

    // LINT.IfChange(ActorPipStatus)

    @IntDef({ActorPipStatus.ENTERED, ActorPipStatus.EXITED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipStatus {
        int ENTERED = 0;
        int EXITED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipStatus)

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

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipExitReason)

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

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipUserInteraction)

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

    private ActorMetrics() {}

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
            RecordHistogram.recordEnumeratedHistogram(
                    "Actor.Notification.ClickTaskState", state, ActorTaskState.MAX_VALUE + 1);
        }

        // Consume the extras so that we don't record the same intent multiple times.
        intent.removeExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID);
        intent.removeExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE);
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
            tracker = new LatencyTracker(newState, mCurrentGlobalMode);
            mTrackers.put(taskId, tracker);
        } else {
            tracker.updateTaskStateAndMode(newState, mCurrentGlobalMode);
        }

        if (ActorUtils.isCompletedState(newState)) {
            tracker.recordTaskMetrics();
            mTrackers.remove(taskId);
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
    }

    public void onTaskStateChangedForTesting(
            @ActorTaskId int taskId, @ActorTaskState int newState) {
        onTaskStateChanged(taskId, newState);
    }
}
