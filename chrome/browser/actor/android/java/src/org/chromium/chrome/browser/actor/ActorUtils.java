// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.NotificationChannel;
import android.app.NotificationManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/** Utility methods for Actor tasks. */
@NullMarked
public class ActorUtils {
    /**
     * Determines whether a notification should be ongoing. Ongoing status is used for live
     * notifications and is gated on {@link ChromeFeatureList#sActorLiveNotification}, which
     * controls Android 16 live notification (promoted ongoing) support.
     *
     * @param isLive Whether the notification is requested to be live.
     * @return True if the notification should be ongoing.
     */
    public static boolean isOngoingNotification(boolean isLive) {
        return isLive && ChromeFeatureList.sActorLiveNotification.isEnabled();
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is completed (finished, failed, or cancelled).
     */
    public static boolean isCompletedState(@ActorTaskState int state) {
        return state == ActorTaskState.FINISHED
                || state == ActorTaskState.FAILED
                || state == ActorTaskState.CANCELLED;
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is a stopped terminal state (failed or cancelled).
     */
    public static boolean isStoppedState(@ActorTaskState int state) {
        return state == ActorTaskState.FAILED || state == ActorTaskState.CANCELLED;
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is a running/working state (created, acting, or reflecting).
     */
    public static boolean isRunningState(@ActorTaskState int state) {
        return state == ActorTaskState.CREATED
                || state == ActorTaskState.ACTING
                || state == ActorTaskState.REFLECTING;
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is a paused state (paused by actor, paused by user).
     */
    public static boolean isPausedState(@ActorTaskState int state) {
        return state == ActorTaskState.PAUSED_BY_ACTOR || state == ActorTaskState.PAUSED_BY_USER;
    }

    /**
     * @param prevTaskState The first {@link ActorTaskState} to compare.
     * @param newTaskState The second {@link ActorTaskState} to compare.
     * @return True if both states belong to the same logical group (Running, Paused, Waiting on
     *     User, or Completed).
     */
    public static boolean isSameLogicalGroup(
            @ActorTaskState int prevTaskState, @ActorTaskState int newTaskState) {
        if (prevTaskState == newTaskState) return true;
        if (isRunningState(prevTaskState) && isRunningState(newTaskState)) return true;
        if (isPausedState(prevTaskState) && isPausedState(newTaskState)) return true;
        if (isCompletedState(prevTaskState) && isCompletedState(newTaskState)) return true;
        return prevTaskState == ActorTaskState.WAITING_ON_USER
                && newTaskState == ActorTaskState.WAITING_ON_USER;
    }

    /** Returns whether both app-level notifications and the Actor channel are enabled. */
    public static boolean areActorNotificationsEnabled() {
        if (!NotificationProxyUtils.areNotificationsEnabled()) {
            return false;
        }
        NotificationChannel channel =
                NotificationManagerProxyImpl.getInstance()
                        .getNotificationChannel(ChromeChannelDefinitions.ChannelId.ACTOR);
        return channel == null || channel.getImportance() != NotificationManager.IMPORTANCE_NONE;
    }

    /**
     * Returns whether background actuation is enabled and allowed (i.e. the base
     * GlicBackgroundActuation feature flag is enabled, and either notifications are enabled or the
     * feature param is configured not to require notifications).
     */
    public static boolean isBackgroundActuationEnabled() {
        if (!ChromeFeatureList.sGlicBackgroundActuation.isEnabled()) {
            return false;
        }
        return !ChromeFeatureList.sGlicBackgroundActuationRequireNotifications.getValue()
                || areActorNotificationsEnabled();
    }
}
