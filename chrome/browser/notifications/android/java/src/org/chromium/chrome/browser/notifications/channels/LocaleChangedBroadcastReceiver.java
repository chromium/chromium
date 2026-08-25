// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Process;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;

/** Triggered when Android's locale changes. */
@NullMarked
public class LocaleChangedBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "LocaleChangeReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!Intent.ACTION_LOCALE_CHANGED.equals(intent.getAction())) return;
        updateChannels();
    }

    /** Updates notification channels to reflect the new locale. */
    private void updateChannels() {
        final PendingResult result = goAsync();
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    ChannelsUpdater.getInstance().updateLocale();
                    result.finish();

                    // See https://crbug.com/545907093 for why we restart on locale change.
                    if (ApplicationLifetime.shouldRestartForLocaleSwitch()) {
                        Log.e(TAG, "Restarting process because of settings locale change.");
                        // This task is on a background thread, so post back to the UI thread.
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> ApplicationLifetime.terminate(/* restart= */ true));
                    } else {
                        Log.e(TAG, "Killing process because of OS locale change.");
                        Process.killProcess(Process.myPid());
                    }
                });
    }
}
