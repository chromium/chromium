// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;
import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;

import java.util.Set;

/**
 * Interface for controlling the ActorForegroundService lifecycle and interaction from the browser
 * layer.
 */
@NullMarked
public interface ActorForegroundServiceController {
    /** Starts the service. */
    default void startService() {
        startService("");
    }

    /**
     * Starts the service.
     *
     * @param glicTriggerMessageId The GLIC trigger message ID associated with the request.
     */
    void startService(String glicTriggerMessageId);

    /**
     * Starts the service and binds to it.
     *
     * @param onConnected Runnable to be called when the service is connected.
     */
    void startAndBindService(Runnable onConnected);

    /** Unbinds from the service. */
    void unbindService();

    /** Returns whether the service is currently bound and connected. */
    boolean isConnected();

    /**
     * Starts or updates the foreground service with a notification.
     *
     * @param newNotificationId The ID for the new notification.
     * @param newNotification The notification to display.
     * @param oldNotificationId The ID of the previous notification, or -1 if none.
     * @param killOldNotification Whether to remove the old notification.
     */
    void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification);

    /** Proxies the stopActorForegroundService call to the bound service. */
    void stopActorForegroundService(int flags);

    /**
     * Transitions active tasks from foreground activity to background rendering.
     *
     * @param selector The TabModelSelector of the stopping activity.
     */
    default void transitionActiveTasksToBackground(TabModelSelector selector) {}

    /** Restores active window background tabs when activity starts or receives intent. */
    default void restoreActiveWindowBackgroundTabs(
            TabModelSelector selector,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory) {}

    /** Handles cleanup when a message-triggered task is stopped for a context ID. */
    default void onMessageTriggerTaskStopped(String contextId) {}

    /**
     * Handles task completion by persisting associated background tabs to disk and restoring warm
     * sessions.
     *
     * @param taskId The ID of the completed task.
     */
    default void onTaskCompleted(int taskId) {}

    /** Destroys the background actuation manager and cleans up its resources. */
    default void destroyBackgroundActuationManager() {}

    /**
     * Provisions an offscreen tab on demand for the specified task ID.
     *
     * @param profile The profile to use.
     * @param taskId The task ID.
     * @param callback Callback invoked with the prepared tab, or null if setup failed.
     */
    default void provisionBackgroundTabForTask(
            Profile profile, int taskId, Callback<@Nullable Tab> callback) {
        callback.onResult(null);
    }

    /**
     * Returns the placeholder tab ID associated with a given original tab ID if it's currently in a
     * background session.
     */
    default @Nullable Integer getPlaceholderTabIdForTaskTab(int originalTabId) {
        return null;
    }

    /**
     * Creates an Intent that tells Chrome to bring an Activity for a particular Tab back to the
     * foreground and show the actor control bottom sheet.
     *
     * @param task The {@link ActorTask} to bring to front.
     * @return Created Intent.
     */
    @Nullable Intent createTrustedBringTabToFrontIntent(ActorTask task);

    /**
     * Returns true if there is a visible Chrome activity that has one of the tabs, the given task
     * is acting on.
     */
    boolean isActivityVisibleForTabs(Set<Integer> tabIds);

    /** Returns true if a tabbed activity is currently visible. */
    boolean isTabbedActivityVisible();

    /** Returns the singleton instance. */
    static ActorForegroundServiceController get() {
        if (Holder.sInstanceForTesting != null) return Holder.sInstanceForTesting;
        if (Holder.sInstance != null) return Holder.sInstance;
        ActorForegroundServiceController ret =
                ServiceLoaderUtil.maybeCreate(ActorForegroundServiceController.class);
        if (ret == null) {
            ret = NoOpActorForegroundServiceController.getInstance();
        }
        Holder.sInstance = ret;
        return ret;
    }

    static void setInstanceForTesting(ActorForegroundServiceController controller) {
        Holder.sInstanceForTesting = controller;
        ResettersForTesting.register(() -> Holder.sInstanceForTesting = null);
    }

    class Holder {
        static @Nullable ActorForegroundServiceController sInstance;
        static @Nullable ActorForegroundServiceController sInstanceForTesting;
    }
}
