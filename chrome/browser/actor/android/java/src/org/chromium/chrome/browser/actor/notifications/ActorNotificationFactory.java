// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.actor.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationConfig.TaskSnapshot;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
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

    // Constants for Android 16 (API 36) Promoted Ongoing (Live Updates) notifications.
    public static final String EXTRA_REQUEST_PROMOTED_ONGOING =
            Notification.EXTRA_REQUEST_PROMOTED_ONGOING;
    public static final String EXTRA_SHORT_CRITICAL_TEXT =
            NotificationCompat.EXTRA_SHORT_CRITICAL_TEXT;

    public static final int TASK_STARTS_SOON_NOTIFICATION_ID = 101;
    public static final int OPT_IN_NOTIFICATION_ID = 102;

    @IntDef({
        NotificationCategory.RUNNING,
        NotificationCategory.PAUSED,
        NotificationCategory.USER_INPUT,
        NotificationCategory.SUCCESS,
        NotificationCategory.STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface NotificationCategory {
        int RUNNING = 0;
        int PAUSED = 1;
        int USER_INPUT = 2;
        int SUCCESS = 3;
        int STOPPED = 4;
    }

    /**
     * Builds a notification for an actor task with an explicit state and warning mode.
     *
     * @param task The {@link ActorTask} to build the notification for.
     * @param state The {@link ActorTaskState} of the task.
     * @param isSilent Whether the notification should be silent.
     * @param isWarning Whether the notification is in a warning state.
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildNotification(
            ActorTask task, @ActorTaskState int state, boolean isSilent, boolean isWarning) {
        return buildNotification(task, state, isSilent, isWarning, /* isLive= */ true);
    }

    /**
     * Builds a notification for an actor task with an explicit state, warning mode, and live
     * status.
     *
     * @param task The {@link ActorTask} to build the notification for.
     * @param state The {@link ActorTaskState} of the task.
     * @param isSilent Whether the notification should be silent.
     * @param isWarning Whether the notification is in a warning state.
     * @param isLive Whether the notification should be configured as a live notification.
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildNotification(
            ActorTask task,
            @ActorTaskState int state,
            boolean isSilent,
            boolean isWarning,
            boolean isLive) {
        TaskSnapshot snapshot =
                TaskSnapshot.of(
                        task,
                        state,
                        ChromeFeatureList.sActorStepProgressNotification.isEnabled()
                                        && ActorUtils.isRunningState(state)
                                ? task.getCurrentActionName()
                                : null,
                        /* isExperimentalTriggeringSilenceEnabled= */ false);
        return buildNotification(
                ActorNotificationConfig.forResolvedTask(
                        ContextUtils.getApplicationContext(),
                        snapshot,
                        isWarning,
                        isLive,
                        ActorUtils.isLiveNotificationEnabled(),
                        isSilent),
                task);
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
        if (oldState == ActorTaskState.CREATED && newState != ActorTaskState.CREATED) {
            return true;
        }
        return getNotificationCategory(oldState) != getNotificationCategory(newState);
    }

    static @NotificationCategory int getNotificationCategory(@ActorTaskState int state) {
        if (ActorUtils.isRunningState(state)) {
            return NotificationCategory.RUNNING;
        }
        if (ActorUtils.isPausedState(state)) {
            return NotificationCategory.PAUSED;
        }
        if (state == ActorTaskState.WAITING_ON_USER) return NotificationCategory.USER_INPUT;
        if (state == ActorTaskState.FINISHED) return NotificationCategory.SUCCESS;
        return NotificationCategory.STOPPED;
    }

    /**
     * Builds the notification described by {@code config}.
     *
     * @param config The resolved {@link ActorNotificationConfig} of the notification.
     * @param task The task whose tab the notification routes to.
     * @return The built {@link NotificationWrapper}.
     */
    static NotificationWrapper buildNotification(ActorNotificationConfig config, ActorTask task) {
        Context context = ContextUtils.getApplicationContext();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.ACTOR,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.ACTOR,
                                        config.getNotificationTag(),
                                        config.getNotificationId()))
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setSilent(config.isSilent())
                        .setContentTitle(context.getString(config.getTitleRes()));
        if (config.isOnlyAlertOnce()) {
            builder.setOnlyAlertOnce(true);
        }
        if (config.isHighPriority()) {
            builder.setPriorityBeforeO(Notification.PRIORITY_HIGH);
        }
        String body = config.getBody();
        builder.setContentText(body).setBigTextStyle(body);
        addTaskContent(builder, context, config, task);
        return builder.buildNotificationWrapper();
    }

    private static void addTaskContent(
            NotificationWrapperBuilder builder,
            Context context,
            ActorNotificationConfig config,
            ActorTask task) {
        builder.setLocalOnly(true);
        boolean isOngoing = config.isOngoing();
        if (isOngoing) {
            Bundle extras = new Bundle();
            extras.putBoolean(EXTRA_REQUEST_PROMOTED_ONGOING, true);
            if (config.getChipRes() != 0) {
                extras.putCharSequence(
                        EXTRA_SHORT_CRITICAL_TEXT, context.getString(config.getChipRes()));
            }
            builder.addExtras(extras);
        }
        if (config.isTerminal()) {
            builder.setAutoCancel(true).setOngoing(isOngoing);
        } else {
            builder.setOngoing(true);
        }

        int notificationId = config.getNotificationId();
        // TODO(crbug.com/564644170): Reuse one routing intent for the content and the action once
        // tests stop expecting one createTrustedBringTabToFrontIntent() call for each.
        builder.setContentIntent(createTabRoutingIntent(context, notificationId, task));
        @Nullable PendingIntentProvider actionIntent =
                createTabRoutingIntent(context, notificationId, task);
        if (actionIntent == null) return;
        builder.addAction(
                R.drawable.ic_chrome,
                context.getString(R.string.actor_notification_button_go_to_chrome),
                actionIntent,
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

    /**
     * Builds a notification to show that the foreground service has started and the task will start
     * soon.
     *
     * @param isSilent Whether the notification should be silent.
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildTaskStartsSoonNotification(boolean isSilent) {
        Context context = ContextUtils.getApplicationContext();
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.ACTOR,
                        /* notificationTag= */ null,
                        TASK_STARTS_SOON_NOTIFICATION_ID);
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.ACTOR, metadata)
                .setSmallIcon(R.drawable.ic_chrome)
                .setSilent(isSilent)
                .setContentTitle(
                        context.getString(
                                R.string.actor_notification_title_preparing_to_start_task))
                .buildNotificationWrapper();
    }

    /**
     * Builds the system notification shown when a remote experimental triggering opt-in request
     * arrives while Chrome is in the background.
     *
     * @param isPinned Whether the notification is pinned to the foreground service (untagged) or
     *     posted unpinned (tagged under {@link
     *     ActorNotificationService#UNPINNED_NOTIFICATION_TAG}).
     * @param isSilent Whether the notification should be silent (e.g. when transitioning between
     *     pinned and unpinned states after already alerting the user).
     * @return The built {@link NotificationWrapper}.
     */
    public static NotificationWrapper buildOptInNotification(boolean isPinned, boolean isSilent) {
        Intent intent =
                ActorForegroundServiceController.get().createTrustedPendingActorTaskNewTabIntent();
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent =
                PendingIntentProvider.getActivity(
                        context, OPT_IN_NOTIFICATION_ID, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        Intent cancelIntent =
                new Intent(context, ActorBroadcastReceiver.class)
                        .setAction(NotificationConstants.ACTION_ACTOR_CANCEL);
        PendingIntentProvider cancelPendingIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        OPT_IN_NOTIFICATION_ID,
                        cancelIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT);

        String title = context.getString(R.string.glic_experimental_opt_in_notification_title);
        String body = context.getString(R.string.glic_experimental_opt_in_notification_body);

        String notificationTag =
                isPinned ? null : ActorNotificationService.UNPINNED_NOTIFICATION_TAG;
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.ACTOR,
                        notificationTag,
                        OPT_IN_NOTIFICATION_ID);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.ACTOR, metadata)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setLocalOnly(true)
                        .setAutoCancel(!isPinned)
                        .setOngoing(isPinned)
                        .setSilent(isSilent)
                        .setPriorityBeforeO(Notification.PRIORITY_HIGH)
                        .setCategory(Notification.CATEGORY_RECOMMENDATION)
                        .setContentTitle(title)
                        .setContentText(body)
                        .setBigTextStyle(body)
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(cancelPendingIntent)
                        .addAction(
                                R.drawable.ic_chrome,
                                context.getString(
                                        R.string.glic_experimental_opt_in_notification_action),
                                contentIntent,
                                NotificationUmaTracker.ActionType.ACTOR_VIEW)
                        .addAction(
                                /* icon= */ 0,
                                context.getString(R.string.no_thanks),
                                cancelPendingIntent,
                                NotificationUmaTracker.ActionType.ACTOR_CANCEL);
        if (isSilent) {
            builder.setOnlyAlertOnce(true);
        }
        return builder.buildNotificationWrapper();
    }
}
