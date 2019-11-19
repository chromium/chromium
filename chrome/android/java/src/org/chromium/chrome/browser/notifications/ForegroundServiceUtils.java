// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.support.v4.app.ServiceCompat;
import android.support.v4.content.ContextCompat;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.AppHooks;

/**
 * Utility functions that call into Android foreground service related API, and provides
 * compatibility for older Android versions and work around for Android API bugs.
 */
public class ForegroundServiceUtils {
    private static final String TAG = "ForegroundService";
    private ForegroundServiceUtils() {}

    /**
     * Gets the singleton instance of ForegroundServiceUtils.
     */
    public static ForegroundServiceUtils getInstance() {
        return ForegroundServiceUtils.LazyHolder.sInstance;
    }

    /**
     * Sets a mocked instance for testing.
     */
    @VisibleForTesting
    public static void setInstanceForTesting(ForegroundServiceUtils instance) {
        ForegroundServiceUtils.LazyHolder.sInstance = instance;
    }

    private static class LazyHolder {
        private static ForegroundServiceUtils sInstance = new ForegroundServiceUtils();
    }

    /**
     * Starts a service from {@code intent} with the expectation that it will make itself a
     * foreground service with {@link android.app.Service#startForeground(int, Notification)}.
     *
     * @param intent The {@link Intent} to fire to start the service.
     */
    public void startForegroundService(Intent intent) {
        ContextCompat.startForegroundService(ContextUtils.getApplicationContext(), intent);
    }

    /**
     * Upgrades a service from background to foreground after calling
     * {@link #startForegroundService(Intent)}.
     * @param service The service to be foreground.
     * @param id The notification id.
     * @param notification The notification attached to the foreground service.
     * @param foregroundServiceType The type of foreground service. Must be a subset of the
     *                              foreground service types defined in AndroidManifest.xml.
     *                              Use 0 if no foregroundServiceType attribute is defined.
     */
    public void startForeground(
            Service service, int id, Notification notification, int foregroundServiceType) {
        // If android fail to build the notification, do nothing.
        if (notification == null) return;

        // TODO(xingliu): Remove startForeground call from AppHooks when Q sdk is available.
        AppHooks.get().startForeground(service, id, notification, foregroundServiceType);
    }

    /**
     * Stops the foreground service. See {@link ServiceCompat#stopForeground(Service, int)}.
     * @param service The foreground service to stop.
     * @param flags The flags to stop foreground service.
     */
    public void stopForeground(Service service, int flags) {
        // OnePlus devices may throw NullPointerException, see https://crbug.com/992347.
        try {
            ServiceCompat.stopForeground(service, flags);
        } catch (NullPointerException e) {
            Log.e(TAG, "Failed to stop foreground service, ", e);
        }
    }
}
