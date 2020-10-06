// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.base.SplitCompatIntentService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link NotificationServiceImpl}. */
public class NotificationService extends SplitCompatIntentService {
    private static final String TAG = NotificationService.class.getSimpleName();

    public NotificationService() {
        super(SplitCompatUtils.getIdentifierName(
                      "org.chromium.chrome.browser.notifications.NotificationServiceImpl"),
                TAG);
    }

    /**
     * This receiver forwards the onReceive() call to the implementation version. This is needed to
     * handle pending intents referring to the old receiver name.
     */
    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            BroadcastReceiver receiver = (BroadcastReceiver) SplitCompatUtils.newInstance(context,
                    "org.chromium.chrome.browser.notifications.NotificationServiceImpl$Receiver");
            receiver.onReceive(context, intent);
        }
    }
}
