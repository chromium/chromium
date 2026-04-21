// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.HashMap;
import java.util.Map;

/**
 * Manages the state and display of notifications for Actor tasks. Notifications are only surfaced
 * to the system tray when Chrome is in PiP mode, but one notification is always pinned to the
 * ForegroundService when tasks are active.
 */
@NullMarked
public class ActorNotificationService {
    private final Map<Integer, ActorTask> mTaskCache = new HashMap<>();
    private final Map<Integer, NotificationWrapper> mNotificationCache = new HashMap<>();
    private final Map<Integer, Integer> mTaskStates = new HashMap<>();
    private static final String TAG = "ActorNotification";
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
     * @return The notification to be used for the foreground service, or null if the task is null.
     */
    public @Nullable Notification getForegroundNotification(
            @Nullable ActorTask task, boolean isSilent) {
        if (task == null) return null;

        // Currently, we only support pinning one task's notification.
        // In the future, this can be extended to return a grouped summary notification.
        return getCachedNotification(task.getId(), isSilent);
    }

    /**
     * Updates the internal notification state for a specific task based on its new state.
     *
     * @param taskId The ID of the task that changed state.
     * @param newState The new state of the task.
     * @param isSilent Whether the notification should be silent or popup.
     */
    public void updateNotificationForTask(
            int taskId, @ActorTaskState int newState, boolean isSilent) {
        ActorTask task = getTask(taskId);
        if (task == null) {
            mNotificationManager.cancel(taskId);
            clearTaskData(taskId);
            return;
        }

        Integer oldState = mTaskStates.get(taskId);

        // If the task appears to have regressed to CREATED from a terminal state, it's likely
        // because the native task was destroyed. In this case, we trust our cached state.
        if (oldState != null && isTerminalState(oldState) && newState == ActorTaskState.CREATED) {
            newState = oldState;
        }

        if (oldState != null
                && !ActorNotificationFactory.shouldUpdateNotification(oldState, newState)) {
            mTaskStates.put(taskId, newState);
            return;
        }

        NotificationWrapper notification =
                ActorNotificationFactory.buildNotification(task, newState, isSilent);
        mNotificationManager.notify(notification);
        mNotificationCache.put(taskId, notification);
        mTaskStates.put(taskId, newState);
    }

    /**
     * Retrieves the cached notification for a task, or creates a new one if it doesn't exist.
     *
     * @param taskId The ID of the task.
     * @param isSilent Whether the notification should be silent or popup if it needs to be built.
     * @return The {@link Notification} object, or null if the task cannot be found.
     */
    @Nullable
    public Notification getCachedNotification(int taskId, boolean isSilent) {
        NotificationWrapper wrapper = getNotificationWrapper(taskId, isSilent);
        return wrapper != null ? wrapper.getNotification() : null;
    }

    private @Nullable NotificationWrapper getNotificationWrapper(int taskId, boolean isSilent) {
        ActorTask task = getTask(taskId);
        if (task == null) return null;

        @ActorTaskState int state = task.getState();
        NotificationWrapper cachedNotification = mNotificationCache.get(taskId);
        Integer oldState = mTaskStates.get(taskId);

        if (oldState != null && isTerminalState(oldState) && state == ActorTaskState.CREATED) {
            state = oldState;
        }

        if (cachedNotification == null
                || oldState == null
                || ActorNotificationFactory.shouldUpdateNotification(oldState, state)) {
            cachedNotification = ActorNotificationFactory.buildNotification(task, state, isSilent);
            mNotificationCache.put(taskId, cachedNotification);
            mTaskStates.put(taskId, state);
        }
        return cachedNotification;
    }

    private boolean isTerminalState(@ActorTaskState int state) {
        return state == ActorTaskState.FINISHED
                || state == ActorTaskState.FAILED
                || state == ActorTaskState.CANCELLED;
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
        mNotificationCache.clear();
        mTaskStates.clear();
        mTaskCache.clear();
    }

    private void clearTaskData(int taskId) {
        mNotificationCache.remove(taskId);
        mTaskStates.remove(taskId);
        mTaskCache.remove(taskId);
    }
}
