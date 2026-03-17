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
    private final Map<Integer, NotificationWrapper> mNotificationCache = new HashMap<>();
    private final Map<Integer, Integer> mTaskStates = new HashMap<>();
    private static final String TAG = "ActNotification";
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
     * @return The notification to be used for the foreground service, or null if the task is null.
     */
    public @Nullable Notification getForegroundNotification(@Nullable ActorTask task) {
        if (task == null) return null;

        // Currently, we only support pinning one task's notification.
        // In the future, this can be extended to return a grouped summary notification.
        return getCachedNotification(task.getId());
    }

    /**
     * Updates the internal notification state for a specific task based on its new state.
     *
     * @param taskId The ID of the task that changed state.
     * @param newState The new state of the task.
     */
    public void updateNotificationForTask(int taskId, @ActorTaskState int newState) {
        ActorTask task = mKeyedService.getTask(taskId);
        if (task == null) {
            mNotificationManager.cancel(taskId);
            mNotificationCache.remove(taskId);
            mTaskStates.remove(taskId);
            return;
        }

        Integer oldState = mTaskStates.get(taskId);
        if (oldState != null
                && !ActorNotificationFactory.shouldUpdateNotification(oldState, newState)) {
            mTaskStates.put(taskId, newState);
            return;
        }

        NotificationWrapper notification =
                ActorNotificationFactory.buildNotification(task, newState);
        mNotificationManager.notify(notification);
        mNotificationCache.put(taskId, notification);
        mTaskStates.put(taskId, newState);
    }

    /**
     * Retrieves the cached notification for a task, or creates a new one if it doesn't exist.
     *
     * @param taskId The ID of the task.
     * @return The {@link Notification} object, or null if the task cannot be found.
     */
    @Nullable
    public Notification getCachedNotification(int taskId) {
        NotificationWrapper notification = mNotificationCache.get(taskId);
        if (notification == null) {
            ActorTask task = mKeyedService.getTask(taskId);
            if (task != null) {
                int state = task.getState();
                notification = ActorNotificationFactory.buildNotification(task, state);
                mNotificationCache.put(taskId, notification);
                mTaskStates.put(taskId, state);
            }
        }
        return notification != null ? notification.getNotification() : null;
    }

    /** Cancels all active actor notifications and clears the local cache. */
    public void clearAll() {
        mNotificationManager.cancelAll();
        mNotificationCache.clear();
        mTaskStates.clear();
    }
}
