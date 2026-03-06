// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

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
     * @param primaryTask The task to show the notification for.
     * @return The notification to be used for the foreground service, or null if the task is null.
     */
    public @Nullable Notification getForegroundNotification(@Nullable ActorTask primaryTask) {
        if (primaryTask == null) return null;

        // Currently, we only support pinning one task's notification.
        // In the future, this can be extended to return a grouped summary notification.
        return getCachedNotification(primaryTask.getId());
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
            return;
        }

        // TODO(487671227): Use ActorNotificationFactory to build real notifications.
        // For now, we use a stub notification to establish the pipeline.
        NotificationWrapper notification = createStubNotification(task);
        mNotificationCache.put(taskId, notification);
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
                notification = createStubNotification(task);
                mNotificationCache.put(taskId, notification);
            }
        }
        return notification != null ? notification.getNotification() : null;
    }

    /** Cancels all active actor notifications and clears the local cache. */
    public void clearAll() {
        mNotificationManager.cancelAll();
        mNotificationCache.clear();
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder getNotificationBuilder(int notificationId) {
        // TODO(487982067): Create new channel for Actor.
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.BROWSER,
                new NotificationMetadata(SystemNotificationType.ACTOR, TAG, notificationId));
    }

    private NotificationWrapper createStubNotification(ActorTask task) {
        // TODO(487671227) : Set correct icon and string.
        NotificationWrapperBuilder builder = getNotificationBuilder(task.getId());
        builder.setContentTitle(task.getTitle())
                .setContentText("Gemini is performing task.")
                .setSmallIcon(
                        Icon.createWithBitmap(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)));
        return builder.buildNotificationWrapper();
    }
}
