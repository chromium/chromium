// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;

/** Handles broadcast intents for Actor tasks from notifications. */
@NullMarked
public class ActorBroadcastReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (action == null) return;
        final PendingResult pendingResult = goAsync();

        ProfileIntentUtils.retrieveProfileFromIntent(
                intent,
                (profile) -> {
                    try {
                        int taskId = intent.getIntExtra(ActorIntentConstants.EXTRA_TASK_ID, -1);
                        updateActorTaskForProfile(profile, action, taskId);
                    } finally {
                        pendingResult.finish();
                    }
                });
    }

    private void updateActorTaskForProfile(@Nullable Profile profile, String action, int taskId) {
        if (profile == null) return;

        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
        if (service == null) return;

        ActorTask task = service.getTask(taskId);
        if (task == null) return;

        if (ActorIntentConstants.ACTION_PAUSE.equals(action)) {
            task.pause();
        } else if (ActorIntentConstants.ACTION_RESUME.equals(action)) {
            task.resume();
        }
    }
}
