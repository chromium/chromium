// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** Builds all types of notifications for Actor tasks. */
@NullMarked
public class ActorNotificationFactory {
    /**
     * Builds a notification for an actor task with an state.
     *
     * @param task The {@link ActorTask} to build the notification for.
     * @param state The {@link ActorTaskState} of the task.
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildNotification(ActorTask task, @ActorTaskState int state) {
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
                        .setGroup(NotificationConstants.GROUP_ACTOR)
                        .setLocalOnly(true);

        if (state == ActorTaskState.ACTING || state == ActorTaskState.REFLECTING) {
            return buildRunningNotification(builder, context, task, notificationId);
        } else if (state == ActorTaskState.PAUSED_BY_ACTOR
                || state == ActorTaskState.PAUSED_BY_USER) {
            return buildPausedNotification(builder, context, task, notificationId);
        } else if (state == ActorTaskState.WAITING_ON_USER) {
            return buildUserInputNotification(builder, context, task);
        } else if (state == ActorTaskState.FINISHED) {
            return buildSuccessNotification(builder, context, task);
        } else {
            return buildInterruptedNotification(builder, context, task);
        }
    }

    private static NotificationWrapper buildRunningNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        builder.setOngoing(true)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_working_on_task))
                .setContentText(
                        context.getString(
                                R.string.actor_notification_body_working, task.getTitle()));
        addViewAction(builder, context, task);
        addPauseAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildPausedNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task, int id) {
        builder.setOngoing(true)
                .setContentTitle(context.getString(R.string.actor_notification_title_task_paused))
                .setContentText(
                        context.getString(
                                R.string.actor_notification_body_paused, task.getTitle()));
        addViewAction(builder, context, task);
        addResumeAction(builder, context, id, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildUserInputNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task) {
        builder.setPriorityBeforeO(Notification.PRIORITY_HIGH)
                .setOngoing(true)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_check_your_task))
                .setContentText(
                        context.getString(
                                R.string.actor_notification_body_user_input, task.getTitle()))
                .setContentIntent(createTabRoutingIntent(context, task));
        addViewAction(builder, context, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildSuccessNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task) {
        builder.setAutoCancel(true)
                .setOngoing(false)
                .setContentTitle(
                        context.getString(R.string.actor_notification_title_task_completed))
                .setContentText(
                        context.getString(
                                R.string.actor_notification_body_finished, task.getTitle()));
        addViewAction(builder, context, task);
        return builder.buildNotificationWrapper();
    }

    private static NotificationWrapper buildInterruptedNotification(
            NotificationWrapperBuilder builder, Context context, ActorTask task) {
        return builder.setOngoing(true)
                .setContentTitle(context.getString(R.string.actor_notification_title_task_paused))
                .setContentText(
                        context.getString(
                                R.string.actor_notification_body_interrupted, task.getTitle()))
                .buildNotificationWrapper();
    }

    private static void addPauseAction(
            NotificationWrapperBuilder builder,
            Context context,
            int notificationId,
            ActorTask task) {
        builder.addAction(
                R.drawable.ic_pause_white_24dp,
                context.getString(R.string.actor_notification_button_pause_task),
                createBroadcastIntent(
                        context, NotificationConstants.ACTION_ACTOR_PAUSE, notificationId, task));
    }

    private static void addResumeAction(
            NotificationWrapperBuilder builder,
            Context context,
            int notificationId,
            ActorTask task) {
        builder.addAction(
                R.drawable.ic_play_arrow_white_24dp,
                context.getString(R.string.actor_notification_button_resume_task),
                createBroadcastIntent(
                        context, NotificationConstants.ACTION_ACTOR_RESUME, notificationId, task));
    }

    private static void addViewAction(
            NotificationWrapperBuilder builder, Context context, ActorTask task) {
        builder.addAction(
                R.drawable.ic_spark_24dp,
                context.getString(R.string.actor_notification_button_view_task),
                createTabRoutingIntent(context, task));
    }

    private static PendingIntent createBroadcastIntent(
            Context context, String action, int notificationId, ActorTask task) {
        Intent intent = new Intent(action);
        intent.setPackage(context.getPackageName());
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, task.getId());
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, notificationId);
        Profile profile = task.getProfile();
        if (profile != null) {
            ProfileIntentUtils.addProfileToIntent(profile, intent);
        }
        return PendingIntent.getBroadcast(
                context,
                notificationId,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private static PendingIntent createTabRoutingIntent(Context context, ActorTask task) {
        // Creates intent to launch chrome, call actor task related tab.
        // TODO(crbug.com/486281299): Implement actual tab routing logic.
        Intent intent = new Intent();
        intent.setPackage(context.getPackageName());
        return PendingIntent.getActivity(
                context,
                task.getId(),
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }
}
