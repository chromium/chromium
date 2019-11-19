// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.upgrade;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantModuleEntryProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelsUpdater;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * Triggered when Chrome's package is replaced (e.g. when it is upgraded).
 *
 * Before changing this class, you must understand both the Receiver and Process Lifecycles:
 * http://developer.android.com/reference/android/content/BroadcastReceiver.html#ReceiverLifecycle
 *
 * - This process runs in the foreground as long as {@link #onReceive} is running.  If there are no
 *   other application components running, Android will aggressively kill it.
 *
 * - Because this runs in the foreground, don't add any code that could cause jank or ANRs.
 *
 * - This class immediately cullable by Android as soon as {@link #onReceive} returns. To kick off
 *   longer tasks, you must start a Service.
 */
public final class PackageReplacedBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(final Context context, Intent intent) {
        if (!Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())) return;
        updateChannelsIfNecessary();
        VrModuleProvider.maybeRequestModuleIfDaydreamReady();
        AutofillAssistantModuleEntryProvider.maybeInstallDeferred();
    }

    private void updateChannelsIfNecessary() {
        if (!ChannelsUpdater.getInstance().shouldUpdateChannels()) return;
        final PendingResult result = goAsync();
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            ChannelsUpdater.getInstance().updateChannels();
            result.finish();
        });
    }
}
