// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.annotation.TargetApi;
import android.app.IntentService;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Service that dispatches a GCM message in the background. Launched from ChromeGcmListenerService
 * if we received a high priority push message, as that should allow us to start a background
 * service even if Chrome is not running.
 */
@TargetApi(Build.VERSION_CODES.N)
public class GCMBackgroundService extends IntentService {
    private static final String TAG = "GCMBackgroundService";

    public GCMBackgroundService() {
        super(TAG);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        Bundle extras = intent.getExtras();
        GCMMessage message = GCMMessage.createFromBundle(extras);
        if (message == null) {
            Log.e(TAG, "The received bundle containing message data could not be validated.");
            return;
        }

        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT,
                () -> ChromeGcmListenerService.dispatchMessageToDriver(this, message));
    }
}
