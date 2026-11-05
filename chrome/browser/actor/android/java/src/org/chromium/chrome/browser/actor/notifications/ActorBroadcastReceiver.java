// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.notifications;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorMetrics;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/** Handles broadcast intents for Actor tasks from notifications. */
@NullMarked
public class ActorBroadcastReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (action == null) return;
        if (NotificationConstants.ACTION_ACTOR_CANCEL.equals(action)
                && IntentUtils.safeGetIntExtra(
                                intent,
                                NotificationConstants.EXTRA_ACTOR_TASK_ID,
                                ActorTask.INVALID_TASK_ID)
                        == ActorTask.INVALID_TASK_ID) {
            ActorForegroundServiceManager fgsManager = ActorForegroundServiceManager.getInstance();
            if (fgsManager != null) {
                fgsManager.cancelOptInNotification();
            } else {
                BaseNotificationManagerProxyFactory.create()
                        .cancel(ActorNotificationFactory.OPT_IN_NOTIFICATION_ID);
            }
            return;
        }
        final PendingResult pendingResult = goAsync();

        ProfileIntentUtils.retrieveProfileFromIntent(
                intent,
                (profile) -> {
                    try {
                        int taskId =
                                IntentUtils.safeGetIntExtra(
                                        intent,
                                        NotificationConstants.EXTRA_ACTOR_TASK_ID,
                                        ActorTask.INVALID_TASK_ID);
                        int source =
                                IntentUtils.safeGetIntExtra(
                                        intent,
                                        NotificationConstants.EXTRA_ACTOR_PAUSE_RESUME_SOURCE,
                                        -1);
                        updateActorTaskForProfile(profile, action, taskId, source);
                    } finally {
                        pendingResult.finish();
                    }
                });
    }

    private void updateActorTaskForProfile(
            @Nullable Profile profile, String action, int taskId, int source) {
        if (profile == null) return;

        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
        if (service == null) return;

        ActorTask task = service.getTask(taskId);
        if (task == null) return;

        if (NotificationConstants.ACTION_ACTOR_PAUSE.equals(action)) {
            if (source == ActorMetrics.ActorPauseResumeSource.PIP) {
                ActorMetrics.recordPipUserInteraction(ActorMetrics.ActorPipUserInteraction.PAUSE);
            }
            task.pause();
        } else if (NotificationConstants.ACTION_ACTOR_RESUME.equals(action)) {
            if (source == ActorMetrics.ActorPauseResumeSource.PIP) {
                ActorMetrics.recordPipUserInteraction(ActorMetrics.ActorPipUserInteraction.RESUME);
            }
            task.resume();
        }
    }
}
