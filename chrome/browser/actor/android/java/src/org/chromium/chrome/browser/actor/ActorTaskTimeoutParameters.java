// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/**
 * Provides Finch-configurable timeout parameters for Actor tasks.
 *
 * <p>These parameters govern the lifecycle and state transitions of tasks, dictating how long a
 * task may remain in a specific state before triggering warnings or transitioning to forcefully
 * stopping a task.
 */
@NullMarked
public class ActorTaskTimeoutParameters {
    private static final String PARAM_RUNNING_TIMEOUT_MS = "running_timeout_ms";
    private static final String PARAM_PAUSED_TIMEOUT_MS = "paused_timeout_ms";
    private static final String PARAM_NEEDS_USER_INPUT_TIMEOUT_MS = "needs_user_input_timeout_ms";
    private static final String PARAM_WARNING_TIMEOUT_MS = "warning_timeout_ms";

    private static final int DEFAULT_RUNNING_TIMEOUT_MS = (int) TimeUnit.MINUTES.toMillis(30);
    private static final int DEFAULT_PAUSED_TIMEOUT_MS = (int) TimeUnit.MINUTES.toMillis(5);
    private static final int DEFAULT_NEEDS_USER_INPUT_TIMEOUT_MS =
            (int) TimeUnit.MINUTES.toMillis(5);
    private static final int DEFAULT_WARNING_TIMEOUT_MS = (int) TimeUnit.MINUTES.toMillis(5);

    /**
     * Gets the timeout duration for the {@code RUNNING} state.
     *
     * <p>When this timeout is reached, the task automatically transitions to a paused state.
     * Concurrently, a soft warning notification is triggered and the warning state timer begins.
     *
     * @return The running timeout in milliseconds.
     */
    public static int getRunningTimeoutMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                PARAM_RUNNING_TIMEOUT_MS,
                DEFAULT_RUNNING_TIMEOUT_MS);
    }

    /**
     * Gets the timeout duration for the {@code PAUSED} state.
     *
     * <p>Reaching this timeout triggers a soft warning notification and initiates the warning state
     * timer.
     *
     * @return The paused timeout in milliseconds.
     */
    public static int getPausedTimeoutMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                PARAM_PAUSED_TIMEOUT_MS,
                DEFAULT_PAUSED_TIMEOUT_MS);
    }

    /**
     * Gets the timeout duration for the {@code NEEDS_USER_INPUT} state.
     *
     * <p>Reaching this timeout triggers a soft warning notification and initiates the warning state
     * timer.
     *
     * @return The needs user input timeout in milliseconds.
     */
    public static int getNeedsUserInputTimeoutMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                PARAM_NEEDS_USER_INPUT_TIMEOUT_MS,
                DEFAULT_NEEDS_USER_INPUT_TIMEOUT_MS);
    }

    /**
     * Gets the timeout duration for the warning state.
     *
     * <p>This acts as the final grace period. If this timeout is reached, the task is forcefully
     * stopped.
     *
     * @return The warning timeout in milliseconds.
     */
    public static int getWarningTimeoutMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                PARAM_WARNING_TIMEOUT_MS,
                DEFAULT_WARNING_TIMEOUT_MS);
    }
}
