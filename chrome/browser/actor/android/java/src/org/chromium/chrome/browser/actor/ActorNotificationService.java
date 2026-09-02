// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Manages the state and display of notifications for Actor tasks. When foreground service is
 * running, one notification is always pinned to it.
 */
@NullMarked
public class ActorNotificationService {
    private static final String TAG = "ActorNotification";

    // Delay to demote terminal live notifications to regular dismissible notifications.
    // Matches the toolbar action chip collapse delay (30 seconds).
    public static final long LIVE_NOTIFICATION_DEMOTION_DELAY_MS = TimeUnit.SECONDS.toMillis(30);
    private static long sDemotionDelayMs = LIVE_NOTIFICATION_DEMOTION_DELAY_MS;

    private final Map<Integer, ActorTask> mTaskCache = new HashMap<>();
    private final Map<Integer, NotificationWrapper> mNotificationCache = new HashMap<>();
    private final Map<Integer, Integer> mTaskStates = new HashMap<>();
    private final Map<Integer, Runnable> mDemoteRunnables = new HashMap<>();
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final BaseNotificationManagerProxy mNotificationManager;
    private final ActorKeyedService mKeyedService;

    /**
     * Constructs an {@link ActorNotificationService} instance.
     *
     * @param keyedService The {@link ActorKeyedService} used to retrieve task information when
     *     creating or updating notifications.
     */
    public ActorNotificationService(ActorKeyedService keyedService) {
        mNotificationManager = BaseNotificationManagerProxyFactory.create();
        mKeyedService = keyedService;
    }

    /**
     * Returns the notification that should be pinned to the {@link ActorForegroundService}.
     *
     * @param task The task to show the notification for.
     * @param isSilent Whether the notification should be silent or popup if it needs to be built.
     * @param isWarning Whether the notification is in a warning state.
     * @return The notification to be used for the foreground service, or null if the task is null.
     */
    public @Nullable Notification getForegroundNotification(
            @Nullable ActorTask task, boolean isSilent, boolean isWarning) {
        if (task == null) return null;

        // Currently, we only support pinning one task's notification.
        // In the future, this can be extended to return a grouped summary notification.
        return getCachedNotification(task.getId(), isSilent, isWarning);
    }

    /**
     * Updates the internal notification state for a specific task based on its new state.
     *
     * @param taskId The ID of the task that changed state.
     * @param newState The new state of the task.
     * @param isSilent Whether the notification should be silent or popup.
     * @param isWarning Whether the notification is in a warning state.
     */
    public void updateNotificationForTask(
            int taskId, @ActorTaskState int newState, boolean isSilent, boolean isWarning) {
        cancelDemoteRunnable(taskId);
        NotificationWrapper old = mNotificationCache.get(taskId);
        NotificationWrapper current =
                getOrBuildNotificationWrapper(taskId, newState, isSilent, isWarning);
        if (current == null) {
            mNotificationManager.cancel(taskId);
            clearTaskData(taskId);
            return;
        }

        if (current != old) {
            mNotificationManager.notify(current);
        }

        if (ActorUtils.isOngoingNotification(ActorUtils.isCompletedState(newState))) {
            scheduleDemotion(taskId);
        }
    }

    private void scheduleDemotion(int taskId) {
        Runnable runnable = () -> demoteToNonLiveNotification(taskId);
        mDemoteRunnables.put(taskId, runnable);
        mHandler.postDelayed(runnable, sDemotionDelayMs);
    }

    private void cancelDemoteRunnable(int taskId) {
        Runnable runnable = mDemoteRunnables.remove(taskId);
        if (runnable != null) {
            mHandler.removeCallbacks(runnable);
        }
    }

    @VisibleForTesting
    void demoteToNonLiveNotification(int taskId) {
        if (mDemoteRunnables.remove(taskId) == null) {
            return;
        }
        ActorTask task = getTask(taskId);
        Integer state = mTaskStates.get(taskId);
        if (task == null || state == null || !ActorUtils.isCompletedState(state)) {
            return;
        }

        NotificationWrapper nonLiveWrapper =
                ActorNotificationFactory.buildNotification(
                        task,
                        state,
                        /* isSilent= */ true,
                        /* isWarning= */ false,
                        /* isLive= */ false);
        mNotificationCache.put(taskId, nonLiveWrapper);
        mNotificationManager.notify(nonLiveWrapper);
    }

    /**
     * Updates the notification for a task when its step progress changes.
     *
     * @param taskId The ID of the task whose step progress updated.
     */
    public void updateNotificationForStepProgress(int taskId) {
        if (!ChromeFeatureList.sActorStepProgressNotification.isEnabled()) {
            return;
        }
        ActorTask task = getTask(taskId);
        if (task == null) {
            return;
        }
        @ActorTaskState int state = task.getState();
        NotificationWrapper current =
                ActorNotificationFactory.buildNotification(
                        task, state, /* isSilent= */ true, /* isWarning= */ false);
        mNotificationCache.put(taskId, current);
        mNotificationManager.notify(current);
    }

    /**
     * Resends the notification for a running task as loud (e.g. when moving to background or PiP).
     *
     * @param taskId The ID of the task to resend notification for.
     */
    public void resendWorkingNotificationLoudly(int taskId) {
        ActorTask task = getTask(taskId);
        if (task == null || !ActorUtils.isRunningState(task.getState())) {
            return;
        }
        NotificationWrapper loudNotification =
                ActorNotificationFactory.buildNotification(
                        task, task.getState(), /* isSilent= */ false, /* isWarning= */ false);
        mNotificationCache.put(taskId, loudNotification);
        mTaskStates.put(taskId, task.getState());
        mNotificationManager.notify(loudNotification);
    }

    /**
     * Retrieves the cached notification for a task, or creates a new one if it doesn't exist.
     *
     * @param taskId The ID of the task.
     * @param isSilent Whether the notification should be silent or popup if it needs to be built.
     * @param isWarning Whether the notification is in a warning state.
     * @return The {@link Notification} object, or null if the task cannot be found.
     */
    @Nullable
    public Notification getCachedNotification(int taskId, boolean isSilent, boolean isWarning) {
        NotificationWrapper wrapper =
                getOrBuildNotificationWrapper(taskId, null, isSilent, isWarning);
        return wrapper != null ? wrapper.getNotification() : null;
    }

    private @Nullable NotificationWrapper getOrBuildNotificationWrapper(
            int taskId, @Nullable Integer newState, boolean isSilent, boolean isWarning) {
        ActorTask task = getTask(taskId);
        if (task == null) return null;

        @ActorTaskState int state = newState != null ? newState : task.getState();
        Integer oldState = mTaskStates.get(taskId);
        NotificationWrapper cachedNotification = mNotificationCache.get(taskId);

        boolean shouldUpdate =
                (cachedNotification == null)
                        || isWarning
                        || (oldState == null)
                        || ActorNotificationFactory.shouldUpdateNotification(oldState, state);

        if (shouldUpdate) {
            cachedNotification =
                    ActorNotificationFactory.buildNotification(task, state, isSilent, isWarning);
            mNotificationCache.put(taskId, cachedNotification);
        }

        mTaskStates.put(taskId, state);
        return cachedNotification;
    }

    /**
     * Returns the task with the given ID, checking both the keyed service and the local cache.
     *
     * @param taskId The ID of the task.
     * @return The {@link ActorTask} if found, null otherwise.
     */
    @Nullable ActorTask getTask(int taskId) {
        return mTaskCache.computeIfAbsent(taskId, mKeyedService::getTask);
    }

    /** Clears local cache for all actor notifications. */
    public void clearAll() {
        for (Runnable runnable : mDemoteRunnables.values()) {
            mHandler.removeCallbacks(runnable);
        }
        mDemoteRunnables.clear();
        mNotificationCache.clear();
        mTaskStates.clear();
        mTaskCache.clear();
    }

    private void clearTaskData(int taskId) {
        cancelDemoteRunnable(taskId);
        mNotificationCache.remove(taskId);
        mTaskStates.remove(taskId);
        mTaskCache.remove(taskId);
    }

    public static void setDemotionDelayMsForTesting(long delayMs) {
        sDemotionDelayMs = delayMs;
        ResettersForTesting.register(() -> sDemotionDelayMs = LIVE_NOTIFICATION_DEMOTION_DELAY_MS);
    }

    boolean hasPendingDemotionForTesting(int taskId) {
        return mDemoteRunnables.containsKey(taskId);
    }

    @Nullable NotificationWrapper getCachedNotificationWrapperForTesting(int taskId) {
        return mNotificationCache.get(taskId);
    }
}
