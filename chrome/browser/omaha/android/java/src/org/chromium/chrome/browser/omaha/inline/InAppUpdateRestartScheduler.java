// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.KeyguardManager;
import android.content.Context;
import android.os.PersistableBundle;
import android.os.PowerManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.inline.InAppUpdatePolicy.RestartMode;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.concurrent.TimeUnit;

/**
 * Schedules and cancels the background install task for a downloaded in-app update.
 *
 * <p>Listens to application lifecycle state changes. When Chrome is left (all activities stopped or
 * destroyed) and an in-app update is staged on disk, this scheduler schedules a short-window {@link
 * InAppUpdateRestartTask} to silently complete the update without interrupting the user. If any
 * Chrome activity becomes visible again (running or paused), any pending restart task is canceled.
 */
@NullMarked
public class InAppUpdateRestartScheduler {
    static final String EXTRA_RETRY_COUNT = "foreground_service_retry_count";

    @VisibleForTesting static final int FOREGROUND_SERVICE_RETRY_INITIAL_DELAY_MINUTES = 15;
    private static final int FOREGROUND_SERVICE_RETRY_MAX_DELAY_MINUTES = 240;
    @VisibleForTesting static final int FOREGROUND_SERVICE_RETRY_WINDOW_MINUTES = 5;
    @VisibleForTesting static final int MAX_FOREGROUND_SERVICE_RETRIES = 6;

    private static @Nullable InAppUpdateRestartScheduler sInstance;

    private boolean mIsListenerRegistered;
    private boolean mIsTaskScheduled;

    @VisibleForTesting
    InAppUpdateRestartScheduler() {}

    /** Returns the process-wide singleton instance. Must be called on the UI thread. */
    public static InAppUpdateRestartScheduler getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new InAppUpdateRestartScheduler();
            ResettersForTesting.register(() -> sInstance = null);
        }
        return sInstance;
    }

    /** Overrides the singleton instance for testing. */
    public static void setInstanceForTesting(@Nullable InAppUpdateRestartScheduler instance) {
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = null);
    }

    /**
     * Clears {@link #mIsTaskScheduled} and the persisted scheduled flag when {@link
     * InAppUpdateRestartTask} starts executing, so subsequent foregrounding does not emit a
     * redundant cancellation event.
     *
     * <p>Only operates if {@link #sInstance} is non-null, ensuring that a cold-started job process
     * without UI does not inadvertently initialize this singleton.
     */
    static void onTaskStarted() {
        ThreadUtils.assertOnUiThread();
        InAppUpdatePolicy.setRestartTaskScheduled(false);
        if (sInstance != null) {
            sInstance.mIsTaskScheduled = false;
        }
    }

    /**
     * Starts the scheduler if background restart is enabled and an update is downloaded. Registers
     * the application lifecycle listener if not already registered, and evaluates the current
     * application state (canceling any stale task if running/paused, or scheduling if
     * stopped/destroyed). Must be called on the UI thread.
     */
    public void start() {
        ThreadUtils.assertOnUiThread();
        if (!isBackgroundRestartEnabledAndDownloaded()) {
            if (mIsTaskScheduled || InAppUpdatePolicy.isRestartTaskScheduled()) {
                cancelTask();
            }
            return;
        }

        boolean firstStart = !mIsListenerRegistered;
        if (firstStart) {
            ApplicationStatus.registerApplicationStateListener(this::onApplicationStateChange);
            mIsListenerRegistered = true;
        }

        // Evaluate the current state on the first start (cancels a stale job from a previous
        // process, or schedules if the user already left) and whenever a task is pending;
        // otherwise skip the IPC on warm resumes.
        if (firstStart || mIsTaskScheduled || InAppUpdatePolicy.isRestartTaskScheduled()) {
            onApplicationStateChange(ApplicationStatus.getStateForApplication());
        }
    }

    /** Handles application lifecycle changes. Must be called on the UI thread. */
    @VisibleForTesting
    void onApplicationStateChange(@ApplicationState int newState) {
        ThreadUtils.assertOnUiThread();
        if (InAppUpdatePolicy.getRestartMode() != RestartMode.BACKGROUND) {
            return;
        }

        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            if (mIsTaskScheduled || InAppUpdatePolicy.isRestartTaskScheduled()) {
                cancelTask();
            }
            return;
        }

        if (newState != ApplicationState.HAS_STOPPED_ACTIVITIES
                && newState != ApplicationState.HAS_DESTROYED_ACTIVITIES) {
            return;
        }

        if (!InAppUpdatePolicy.isUpdateDownloaded()) {
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        if (!isScreenInteractiveAndUnlocked(context)) {
            Log.i(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Screen locked or not interactive,"
                            + " skipping.");
            InAppUpdateRestartTask.recordEvent(
                    InAppUpdateBackgroundRestartEvent.SKIPPED_SCREEN_LOCKED);
            return;
        }

        if (IncognitoTabHostUtils.doIncognitoTabsExist()) {
            Log.i(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Incognito tabs open, skipping.");
            InAppUpdateRestartTask.recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_INCOGNITO);
            return;
        }

        scheduleRestartTask(context);
    }

    private static boolean isScreenInteractiveAndUnlocked(Context context) {
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (powerManager != null && !powerManager.isInteractive()) {
            return false;
        }
        KeyguardManager keyguardManager =
                (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        if (keyguardManager != null && keyguardManager.isKeyguardLocked()) {
            return false;
        }
        return true;
    }

    private void scheduleRestartTask(Context context) {
        // Stopping and destroying the last activity arrive as two state changes within
        // milliseconds of each other (e.g. dismiss from Recents). The first one already scheduled
        // the task, so re-scheduling would only reset the delay and log a duplicate event.
        if (mIsTaskScheduled) {
            return;
        }
        int delayMinutes = InAppUpdatePolicy.getBackgroundRestartDelayMinutes();
        int windowMinutes = InAppUpdatePolicy.getBackgroundRestartWindowMinutes();
        if (scheduleTask(context, delayMinutes, windowMinutes, /* retryCount= */ 0)) {
            mIsTaskScheduled = true;
        }
    }

    private static boolean isBackgroundRestartEnabledAndDownloaded() {
        return InAppUpdatePolicy.getRestartMode() == RestartMode.BACKGROUND
                && InAppUpdatePolicy.isUpdateDownloaded();
    }

    /**
     * Schedules a bounded delayed retry after {@link InAppUpdateRestartTask} skips due to an active
     * foreground service (e.g. media playback or downloads). Must be called on the UI thread after
     * the current job execution has finished.
     */
    static void scheduleForegroundServiceRetry(int currentRetryCount) {
        ThreadUtils.assertOnUiThread();
        if (!isBackgroundRestartEnabledAndDownloaded()) {
            return;
        }
        if (ApplicationStatus.isInitialized() && ApplicationStatus.hasVisibleActivities()) {
            return;
        }
        if (IncognitoTabHostUtils.doIncognitoTabsExist()) {
            return;
        }
        // isScreenInteractiveAndUnlocked() is intentionally not re-checked here: the initial
        // schedule already required an interactive unlocked screen, and any visible Chrome activity
        // since then has canceled the chain, so Chrome cannot be the locked top task on retry.
        if (sInstance != null && sInstance.mIsTaskScheduled) {
            return;
        }
        int retryCount = Math.max(0, currentRetryCount);
        if (retryCount >= MAX_FOREGROUND_SERVICE_RETRIES) {
            Log.i(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Foreground service retry cap reached, not"
                            + " rescheduling.");
            return;
        }
        int delayMinutes = getForegroundServiceRetryDelayMinutes(retryCount);
        Context context = ContextUtils.getApplicationContext();
        if (scheduleTask(
                context, delayMinutes, FOREGROUND_SERVICE_RETRY_WINDOW_MINUTES, retryCount + 1)) {
            if (sInstance != null) {
                sInstance.mIsTaskScheduled = true;
            }
        }
    }

    private static int getForegroundServiceRetryDelayMinutes(int retryCount) {
        return Math.min(
                FOREGROUND_SERVICE_RETRY_INITIAL_DELAY_MINUTES << retryCount,
                FOREGROUND_SERVICE_RETRY_MAX_DELAY_MINUTES);
    }

    private static boolean scheduleTask(
            Context context, int delayMinutes, int windowMinutes, int retryCount) {
        long delayMs = TimeUnit.MINUTES.toMillis(delayMinutes);
        long windowEndMs = delayMs + TimeUnit.MINUTES.toMillis(windowMinutes);
        PersistableBundle extras = new PersistableBundle();
        extras.putInt(EXTRA_RETRY_COUNT, retryCount);
        TaskInfo taskInfo =
                TaskInfo.createTask(
                                TaskIds.IN_APP_UPDATE_RESTART_JOB_ID,
                                TaskInfo.OneOffInfo.create()
                                        .setWindowStartTimeMs(delayMs)
                                        .setWindowEndTimeMs(windowEndMs)
                                        .build())
                        .setExtras(extras)
                        .setUpdateCurrent(true)
                        .setIsPersisted(false)
                        .build();
        boolean scheduled =
                BackgroundTaskSchedulerFactory.getScheduler().schedule(context, taskInfo);
        if (scheduled) {
            InAppUpdatePolicy.setRestartTaskScheduled(true);
            Log.i(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Scheduled background restart task with delay"
                            + " %d min (retry %d).",
                    delayMinutes,
                    retryCount);
            InAppUpdateRestartTask.recordEvent(
                    retryCount == 0
                            ? InAppUpdateBackgroundRestartEvent.SCHEDULED
                            : InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED);
        } else {
            Log.w(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Failed to schedule background restart task.");
        }
        return scheduled;
    }

    private static void cancelPendingJob() {
        ThreadUtils.assertOnUiThread();
        InAppUpdatePolicy.setRestartTaskScheduled(false);
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.IN_APP_UPDATE_RESTART_JOB_ID);
    }

    /** Cancels any scheduled background restart task. Must be called on the UI thread. */
    void cancelTask() {
        ThreadUtils.assertOnUiThread();
        cancelPendingJob();
        if (mIsTaskScheduled) {
            mIsTaskScheduled = false;
            Log.i(
                    InAppUpdateController.TAG,
                    "InAppUpdateRestartScheduler: Canceled background restart task.");
            InAppUpdateRestartTask.recordEvent(InAppUpdateBackgroundRestartEvent.CANCELED);
        }
    }

    /**
     * Returns whether there is state remaining from a previously scheduled background restart or
     * downloaded update that should be cleaned up if the feature is disabled.
     */
    public static boolean hasPendingCleanup() {
        return InAppUpdatePolicy.isRestartTaskScheduled() || InAppUpdatePolicy.isUpdateDownloaded();
    }

    /**
     * Performs deferred cleanup when Chrome starts with the in-app update feature disabled,
     * canceling any pending background job and resetting staged state.
     */
    public static void onFeatureDisabled() {
        ThreadUtils.assertOnUiThread();
        if (InAppUpdatePolicy.isRestartTaskScheduled()) {
            if (sInstance != null) {
                sInstance.cancelTask();
            } else {
                cancelPendingJob();
            }
        }
        if (InAppUpdatePolicy.isUpdateDownloaded()) {
            InAppUpdatePolicy.setUpdateDownloaded(false);
        }
    }

    boolean isTaskScheduledForTesting() {
        return mIsTaskScheduled;
    }

    boolean isListenerRegisteredForTesting() {
        return mIsListenerRegistered;
    }
}
