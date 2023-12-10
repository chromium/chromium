// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/** Triggered when Android's locale changes. */
public class LocaleChangedBroadcastReceiver extends BroadcastReceiver {
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
                });
    }
}
