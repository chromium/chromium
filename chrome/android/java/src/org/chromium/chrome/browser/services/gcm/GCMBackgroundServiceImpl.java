// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.base.SplitCompatIntentService;
import org.chromium.components.gcm_driver.GCMMessage;

/**
 * Service that dispatches a GCM message in the background. Launched from ChromeGcmListenerService
 * if we received a high priority push message, as that should allow us to start a background
 * service even if Chrome is not running.
 */
@NullMarked
public class GCMBackgroundServiceImpl extends SplitCompatIntentService.Impl {
    private static final String TAG = "GCMBackgroundService";

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        if (intent == null) {
            return;
        }
        Bundle extras = intent.getExtras();
        if (extras == null) {
            return;
        }
        GCMMessage message = GCMMessage.createFromBundle(extras);
        if (message == null) {
            Log.e(TAG, "The received bundle containing message data could not be validated.");
            return;
        }

        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> ChromeGcmListenerServiceImpl.dispatchMessageToDriver(message));
    }
}
