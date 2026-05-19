// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Builds all types of notifications for Actor tasks. */
@NullMarked
public class ActorNotificationFactory {
    /** Extra for the Intent to show the actor control bottom sheet. */
    public static final String EXTRA_SHOW_ACTOR_CONTROL =
            "org.chromium.chrome.browser.actor.SHOW_ACTOR_CONTROL";

    @IntDef({
        NotificationCategory.RUNNING,
        NotificationCategory.PAUSED,
        NotificationCategory.USER_INPUT,
        NotificationCategory.SUCCESS,
        NotificationCategory.INTERRUPTED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface NotificationCategory {
        int RUNNING = 0;
        int PAUSED = 1;
        int USER_INPUT = 2;
        int SUCCESS = 3;
        int INTERRUPTED = 4;
    }

    /**
     * Builds a notification for an actor task with an explicit state.
     *
     * @param task The {@link ActorTask} to build the notification for.
     * @param state The {@link ActorTaskState} of the task.
     * @param isSilent Whether the notification should be silent.
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildNotification(
            ActorTask task, @ActorTaskState int state, boolean isSilent) {
        int notificationId = task.getId();
        Context context = ContextUtils.getApplicationContext();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.ACTOR,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.ACTOR,
                                        /* notificationTag= */ null,
                                        notificationId))
                        .setSmallIcon(R.drawable.ic_spark_24dp)
                        .setLocalOnly(true)
                        .setSilent(isSilent);

        if (ActorUtils.isRunningState(state)) {
            return buildRunningNotification(builder, context, task, notificationId);
        } else if (ActorUtils.isPausedState(state)) {
            return buildPausedNotification(builder, context, task, notificationId);
        } else if (state == ActorTaskState.WAITING_ON_USER) {
            return buildUserInputNotification(builder, context, task, notificationId);
        } else if (state == ActorTaskState.FINISHED) {
            return buildSuccessNotification(builder, context, task, notificationId);
        } else {
            return buildInterruptedNotification(builder, context, task, notificationId);
        }
    }

    /**
     * Determines whether a notification update is required when transitioning between two states.
     *
     * @param oldState The previous {@link ActorTaskState}.
     * @param newState The new {@link ActorTaskState}.
     * @return True if the notification should be updated, false otherwise.
     */
    public static boolean shouldUpdateNotification(
            @ActorTaskState int oldState, @ActorTaskState int newState) {
        return getNotificationCategory(oldState) != getNotificationCategory(newState);
    }

    private static @NotificationCategory int getNotificationCategory(@ActorTaskState int state) {
        if (ActorUtils.isRunningState(state)) {
            return NotificationCategory.RUNNING;
        }
        if (ActorUtils.isPausedState(state)) {
            return NotificationCategory.PAUSED;
        }
        if (state == ActorTaskState.WAITING_ON_USER) return NotificationCategory.USER_INPUT;
        if (state == ActorTaskState.FINISHED) return NotificationCategory.SUCCESS;
        return NotificationCategory.INTERRUPTED;
    }

    private static NotificationWrapper buildRunningNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        String body = context.getString(R.string.actor_notification_body_working, task.getTitle());
        builder.setOngoing(true)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_working_on_task))
                .setContentText(body)
                .setBigTextStyle(body)
                .setContentIntent(createTabRoutingIntent(context, id, task));
        addViewAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildPausedNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        String body = context.getString(R.string.actor_notification_body_paused, task.getTitle());
        builder.setOngoing(true)
                .setContentTitle(context.getString(R.string.actor_notification_title_task_paused))
                .setContentText(body)
                .setBigTextStyle(body)
                .setContentIntent(createTabRoutingIntent(context, id, task));
        addViewAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildUserInputNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        String body =
                context.getString(R.string.actor_notification_body_user_input, task.getTitle());
        builder.setPriorityBeforeO(Notification.PRIORITY_HIGH)
                .setOngoing(true)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_check_your_task))
                .setContentText(body)
                .setBigTextStyle(body)
                .setContentIntent(createTabRoutingIntent(context, id, task));
        addViewAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildSuccessNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        String body = context.getString(R.string.actor_notification_body_complete, task.getTitle());
        builder.setAutoCancel(true)
                .setOngoing(false)
                .setContentTitle(context.getString(R.string.actor_notification_title_task_complete))
                .setContentText(body)
                .setBigTextStyle(body)
                .setContentIntent(createTabRoutingIntent(context, id, task));
        addViewAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildInterruptedNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        String body =
                context.getString(R.string.actor_notification_body_interrupted, task.getTitle());
        builder.setAutoCancel(true)
                .setOngoing(false)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_task_interrupted))
                .setContentText(body)
                .setBigTextStyle(body)
                .setContentIntent(createTabRoutingIntent(context, id, task));
        return builder.buildNotificationWrapper();
    }

    private static void addViewAction(
            NotificationWrapperBuilder builder,
            Context context,
            int notificationId,
            ActorTask task) {
        @Nullable PendingIntentProvider intent =
                createTabRoutingIntent(context, notificationId, task);
        if (intent == null) return;

        builder.addAction(
                R.drawable.ic_spark_24dp,
                context.getString(R.string.actor_notification_button_go_to_chrome),
                intent,
                NotificationUmaTracker.ActionType.ACTOR_VIEW);
    }

    private static @Nullable PendingIntentProvider createTabRoutingIntent(
            Context context, int notificationId, ActorTask task) {
        Intent intent =
                ActorForegroundServiceController.get().createTrustedBringTabToFrontIntent(task);
        if (intent == null) return null;
        return PendingIntentProvider.getActivity(
                context, notificationId, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
