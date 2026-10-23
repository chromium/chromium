// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.os.Handler;
import android.os.Looper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;

/**
 * Responsible for monitoring Actor task lifecycle transitions and regulating timeouts.
 *
 * <p>This manager listens to state changes and starts/cancels timers based on Finch-configurable
 * parameters. It coordinates with {@link ActorForegroundServiceManager} to trigger warning states
 * and eventual task termination.
 */
@NullMarked
public class ActorTaskTimeoutManager {
    private static final String TAG = "ActorTimeout";
    private static final int INVALID_TASK_ID = -1;

    private final ActorKeyedService mKeyedService;
    private final ActorForegroundServiceManager mForegroundManager;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private int mCurrentTaskId = INVALID_TASK_ID;
    private @Nullable Runnable mActiveTimer;
    private @Nullable @ActorTaskState Integer mLastState;
    private boolean mIsWarningMode;

    public ActorTaskTimeoutManager(
            ActorKeyedService keyedService, ActorForegroundServiceManager foregroundManager) {
        mKeyedService = keyedService;
        mForegroundManager = foregroundManager;
    }

    /**
     * Processes a task state change. Called explicitly by {@link ActorForegroundServiceManager} to
     * ensure deterministic order of operations.
     */
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        if (mCurrentTaskId == INVALID_TASK_ID) {
            mCurrentTaskId = taskId;
        }

        // Only process events for the task we are currently tracking.
        if (taskId != mCurrentTaskId) return;
        if (mLastState != null && ActorUtils.isSameLogicalGroup(mLastState, newState)) return;

        cancelTimer();
        mLastState = newState;

        if (ActorUtils.isCompletedState(newState)) {
            mIsWarningMode = false;
            mCurrentTaskId = INVALID_TASK_ID;
        } else {
            if (ActorUtils.isRunningState(newState)) {
                mIsWarningMode = false;
            }
            startTimerForState(taskId, newState);
        }
    }

    /** Returns whether the given task is currently in warning mode. */
    public boolean isWarningMode(int taskId) {
        return taskId == mCurrentTaskId && mIsWarningMode;
    }

    private void startTimerForState(int taskId, @ActorTaskState int state) {
        int timeoutMs;
        if (ActorUtils.isRunningState(state)) {
            timeoutMs = ActorTaskTimeoutParameters.getRunningTimeoutMs();
        } else if (ActorUtils.isPausedState(state)) {
            timeoutMs = ActorTaskTimeoutParameters.getPausedTimeoutMs();
        } else if (state == ActorTaskState.WAITING_ON_USER) {
            timeoutMs = ActorTaskTimeoutParameters.getNeedsUserInputTimeoutMs();
        } else {
            return;
        }

        scheduleTimer(() -> triggerSoftWarning(taskId, state), timeoutMs);
    }

    private void triggerSoftWarning(int taskId, @ActorTaskState int state) {
        ActorTask task = mKeyedService.getTask(taskId);
        if (task == null) return;

        mIsWarningMode = true;

        if (ActorUtils.isRunningState(state)) {
            // Preemptively update mLastState before pausing.
            // task.pause() fires a synchronous onTaskStateChanged callback. Setting this prevents
            // the state machine from inadvertently resetting the terminal timer we are about to
            // start.
            mLastState = ActorTaskState.PAUSED_BY_ACTOR;
            task.pause();
        } else {
            // Otherwise for tasks that are paused or needs user input, directly refresh the task
            // UI.
            mForegroundManager.refreshTaskUI(taskId, state);
        }
        startTerminalTimer(taskId);
    }

    private void startTerminalTimer(int taskId) {
        scheduleTimer(
                () -> terminateTask(taskId), ActorTaskTimeoutParameters.getWarningTimeoutMs());
    }

    private void terminateTask(int taskId) {
        ActorTask task = mKeyedService.getTask(taskId);
        if (task == null) return;

        mKeyedService.stopTask(taskId, StoppedReason.TIMEOUT);
    }

    private void scheduleTimer(Runnable action, int delayMs) {
        cancelTimer();
        mActiveTimer = action;
        mHandler.postDelayed(action, delayMs);
    }

    private void cancelTimer() {
        if (mActiveTimer != null) {
            mHandler.removeCallbacks(mActiveTimer);
            mActiveTimer = null;
        }
    }
}
