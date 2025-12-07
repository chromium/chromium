// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.upgrade;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.DexFixer;
import org.chromium.chrome.browser.notifications.channels.ChannelsUpdater;

/**
 * Triggered when Chrome's package is replaced (e.g. when it is upgraded).
 *
 * <p>Before changing this class, you must understand both the Receiver and Process Lifecycles:
 * http://developer.android.com/reference/android/content/BroadcastReceiver.html#ReceiverLifecycle
 *
 * <p>- This process runs in the foreground as long as {@link #onReceive} is running. If there are
 * no other application components running, Android will aggressively kill it.
 *
 * <p>- Because this runs in the foreground, don't add any code that could cause jank or ANRs.
 *
 * <p>- This class immediately cullable by Android as soon as {@link #onReceive} returns. To kick
 * off longer tasks, you must start a Service.
 */
@NullMarked
public final class PackageReplacedBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(final Context context, Intent intent) {
        if (!Intent.ACTION_MY_PACKAGE_REPLACED.equals(intent.getAction())) return;

        final PendingResult result = goAsync();
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    if (ChannelsUpdater.getInstance().shouldUpdateChannels()) {
                        ChannelsUpdater.getInstance().updateChannels();
                    }

                    DexFixer.fixDexInBackground();
                    result.finish();
                });
    }
}
