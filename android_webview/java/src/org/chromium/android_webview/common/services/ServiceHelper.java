// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import android.content.Context;
import android.content.Intent;
import android.content.ReceiverCallNotAllowedException;
import android.content.ServiceConnection;

/**
 * Helper methods for working with Services in WebView.
 */
public class ServiceHelper {
    /**
     * Connects to a Service specified by {@code intent} with {@code flags}. This handles edge cases
     * such as attempting to bind from restricted BroadcastReceiver Contexts.
     *
     * @param context the Context to use when binding to the Service.
     * @param intent should specify a Service.
     * @param serviceConnection a ServiceConnection object.
     * @param flags should be {@code 0} or a combination of {@code Context#BIND_*}.
     */
    public static boolean bindService(
            Context context, Intent intent, ServiceConnection serviceConnection, int flags) {
        try {
            return context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
        } catch (ReceiverCallNotAllowedException e) {
            // If we're running in a BroadcastReceiver Context then we cannot bind to Services.
            return false;
        }
    }

    private ServiceHelper() {}
}
