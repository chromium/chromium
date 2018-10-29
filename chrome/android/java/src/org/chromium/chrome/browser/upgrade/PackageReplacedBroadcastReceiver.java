// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.upgrade;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.notifications.channels.ChannelsUpdater;

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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return;
        UpgradeIntentService.startMigrationIfNecessary(context);
    }

    private void updateChannelsIfNecessary() {
        if (!ChannelsUpdater.getInstance().shouldUpdateChannels()) return;

        final PendingResult result = goAsync();
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                ChannelsUpdater.getInstance().updateChannels();
                result.finish();
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }
}
