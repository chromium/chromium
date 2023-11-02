// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Service that dispatches a GCM message in the background. Launched from ChromeGcmListenerService
 * if we received a high priority push message, as that should allow us to start a background
 * service even if Chrome is not running.
 */
@RequiresApi(Build.VERSION_CODES.N)
public class GCMBackgroundServiceImpl extends GCMBackgroundService.Impl {
    private static final String TAG = "GCMBackgroundService";

    @Override
    protected void onHandleIntent(Intent intent) {
        Bundle extras = intent.getExtras();
        GCMMessage message = GCMMessage.createFromBundle(extras);
        if (message == null) {
            Log.e(TAG, "The received bundle containing message data could not be validated.");
            return;
        }

        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT,
                () -> ChromeGcmListenerServiceImpl.dispatchMessageToDriver(message));
    }
}
