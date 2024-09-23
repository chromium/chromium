// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.components.gcm_driver.InstanceIDFlags;
import org.chromium.components.gcm_driver.LazySubscriptionsManager;
import org.chromium.components.gcm_driver.SubscriptionFlagManager;

/** Receives Downstream messages and status of upstream messages from GCM. */
public class ChromeGcmListenerServiceImpl extends ChromeGcmListenerService.Impl {
    private static final String TAG = "ChromeGcmListener";

    @Override
    public void onCreate() {
        ProcessInitializationHandler.getInstance().initializePreNative();
        super.onCreate();
    }

    @Override
    public void onMessageReceived(final String from, final Bundle data) {
        // Dispatch the message to the GCM Driver for native features.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    GCMMessage message = null;
                    try {
                        message = new GCMMessage(from, data);
                    } catch (IllegalArgumentException e) {
                        Log.e(TAG, "Received an invalid GCM Message", e);
                        return;
                    }

                    scheduleOrDispatchMessageToDriver(message);
                });
    }

    @Override
    public void onMessageSent(String msgId) {
        Log.d(TAG, "Message sent successfully. Message id: %s", msgId);
    }

    @Override
    public void onSendError(String msgId, Exception error) {
        Log.w(TAG, "Error in sending message. Message id: %s", msgId, error);
    }

    @Override
    public void onDeletedMessages() {
        // TODO(johnme): Ask GCM to include the subtype in this event.
        Log.w(
                TAG,
                "Push messages were deleted, but we can't tell the Service Worker as we don't"
                        + "know what subtype (app ID) it occurred for.");
    }

    @Override
    public void onNewToken(String token) {
        // TODO(crbug.com/40725597): Figure out if we can use this method or if
        // we need another mechanism that supports multiple FirebaseApp
        // instances.
        Log.d(TAG, "New FCM Token: %s", token);
    }

    /**
     * Returns if we deliver the GCMMessage with a background service by calling
     * Context#startService. This will only work if Android has put us in an allowlist to allow
     * background services to be started.
     */
    private static boolean maybeBypassScheduler(GCMMessage message) {
        // Android only puts us on an allowlist for high priority messages.
        if (message.getOriginalPriority() != GCMMessage.Priority.HIGH) {
            return false;
        }

        final String subscriptionId =
                SubscriptionFlagManager.buildSubscriptionUniqueId(
                        message.getAppId(), message.getSenderId());
        if (!SubscriptionFlagManager.hasFlags(subscriptionId, InstanceIDFlags.BYPASS_SCHEDULER)) {
            return false;
        }

        try {
            Context context = ContextUtils.getApplicationContext();
            Intent intent = new Intent(context, GCMBackgroundService.class);
            intent.putExtras(message.toBundle());
            context.startService(intent);
            return true;
        } catch (IllegalStateException e) {
            // Failed to start service, maybe we're not allowed? Fallback to using
            // BackgroundTaskScheduler to start Chrome.
            Log.e(TAG, "Could not start background service", e);
            return false;
        }
    }

    /**
     * Returns if the |message| is sent from a lazy subscription and we persist it to be delivered
     * the next time Chrome is launched into foreground.
     */
    private static boolean maybePersistLazyMessage(GCMMessage message) {
        if (isFullBrowserLoaded()) {
            return false;
        }

        final String subscriptionId =
                LazySubscriptionsManager.buildSubscriptionUniqueId(
                        message.getAppId(), message.getSenderId());

        boolean isSubscriptionLazy = LazySubscriptionsManager.isSubscriptionLazy(subscriptionId);
        boolean isHighPriority = message.getOriginalPriority() == GCMMessage.Priority.HIGH;
        // TODO(crbug.com/40619931): Add metrics for the new high priority message logic.
        boolean shouldPersistMessage = isSubscriptionLazy && !isHighPriority;
        if (shouldPersistMessage) {
            LazySubscriptionsManager.persistMessage(subscriptionId, message);
        }

        return shouldPersistMessage;
    }

    /**
     * Schedules a background task via Job Scheduler to deliver the |message|. Delivery might get
     * delayed by Android if the device is currently in doze mode.
     */
    private static void scheduleBackgroundTask(GCMMessage message) {
        // TODO(peter): Add UMA for measuring latency introduced by the BackgroundTaskScheduler.
        TaskInfo backgroundTask =
                TaskInfo.createOneOffTask(TaskIds.GCM_BACKGROUND_TASK_JOB_ID, /* immediately= */ 0)
                        .setExtras(message.toPersistableBundle())
                        .build();
        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), backgroundTask);
    }

    private static void recordWebPushMetrics(GCMMessage message) {
        Context context = ContextUtils.getApplicationContext();
        boolean inIdleMode = DeviceConditions.isCurrentlyInIdleMode(context);
        boolean isHighPriority = message.getOriginalPriority() == GCMMessage.Priority.HIGH;

        @GcmUma.WebPushDeviceState int state;
        if (inIdleMode) {
            state =
                    isHighPriority
                            ? GcmUma.WebPushDeviceState.IDLE_HIGH_PRIORITY
                            : GcmUma.WebPushDeviceState.IDLE_NOT_HIGH_PRIORITY;
        } else {
            state =
                    isHighPriority
                            ? GcmUma.WebPushDeviceState.NOT_IDLE_HIGH_PRIORITY
                            : GcmUma.WebPushDeviceState.NOT_IDLE_NOT_HIGH_PRIORITY;
        }
        GcmUma.recordWebPushReceivedDeviceState(state);
    }

    /**
     * If Chrome is backgrounded, messages coming from lazy subscriptions are
     * persisted on disk and replayed next time Chrome is forgrounded. If Chrome is forgrounded or
     * if the message isn't coming from a lazy subscription, this method either schedules |message|
     * to be dispatched through the Job Scheduler, which we use on Android N and beyond, or
     * immediately dispatches the message on other versions of Android. Some subscriptions bypass
     * the Job Scheduler and use Context#startService instead if the |message| has a high priority.
     * Must be called on the UI thread both for the BackgroundTaskScheduler and for dispatching the
     * |message| to the GCMDriver.
     */
    static void scheduleOrDispatchMessageToDriver(GCMMessage message) {
        ThreadUtils.assertOnUiThread();

        // GCMMessage#getAppId never returns null.
        if (message.getAppId().startsWith("wp:")) {
            recordWebPushMetrics(message);
        }

        // Check if we should only persist the message for now.
        if (maybePersistLazyMessage(message)) {
            return;
        }

        // Check if we should bypass the scheduler for high priority messages.
        if (!maybeBypassScheduler(message)) {
            scheduleBackgroundTask(message);
        }
    }

    /**
     * To be called when a GCM message is ready to be dispatched. Will initialise the native code
     * of the browser process, and forward the message to the GCM Driver. Must be called on the UI
     * thread.
     */
    static void dispatchMessageToDriver(GCMMessage message) {
        ThreadUtils.assertOnUiThread();
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        GCMDriver.dispatchMessage(message);
    }

    private static boolean isFullBrowserLoaded() {
        return ChromeBrowserInitializer.getInstance().isFullBrowserInitialized();
    }
}
