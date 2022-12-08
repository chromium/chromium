// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import android.content.Context;
import android.content.Intent;
import android.content.ReceiverCallNotAllowedException;
import android.content.ServiceConnection;
import android.os.Build;

import org.chromium.base.Log;

/**
 * Helper methods for working with Services in WebView.
 */
public class ServiceHelper {
    private static final String TAG = "ServiceHelper";

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
            return context.bindService(intent, serviceConnection, flags);
        } catch (ReceiverCallNotAllowedException e) {
            // If we're running in a BroadcastReceiver Context then we cannot bind to Services.
            return false;
        } catch (SecurityException e) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                    && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                // There's a known issue on Android N where a secondary user account may not have
                // permission to view the system WebView provider app (most likely, this is
                // Monochrome). In this case, we cannot bind to services so we just log the
                // exception and carry on.
                Log.e(TAG, "Unable to bind to services from a secondary user account on Android N",
                        e);
                return false;
            } else {
                throw e;
            }
        }
    }

    private ServiceHelper() {}
}
