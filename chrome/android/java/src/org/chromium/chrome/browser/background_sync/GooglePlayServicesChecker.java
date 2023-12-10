// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.externalauth.ExternalAuthUtils;

/**
 * Used to check whether Google Play Services version on the device is as
 * expected for BackgroundSync. This check is made at browser startup.
 */
final class GooglePlayServicesChecker {
    private static final String TAG = "PlayServicesChecker";

    private GooglePlayServicesChecker() {}

    /**
     * Returns true if the Background Sync Manager should be automatically disabled
     * on startup. This is currently only the case if Play Services is not up to
     * date, since any sync attempts which fail cannot be reregistered. Better to
     * wait until Play Services is updated before attempting them.
     */
    @CalledByNative
    @VisibleForTesting
    static boolean shouldDisableBackgroundSync() {
        boolean isAvailable = true;
        if (!ExternalAuthUtils.getInstance().canUseGooglePlayServices()) {
            Log.i(TAG, "Disabling Background Sync because Play Services is not up to date.");
            isAvailable = false;
        }

        RecordHistogram.recordBooleanHistogram(
                "BackgroundSync.LaunchTask.PlayServicesAvailable", isAvailable);
        return !isAvailable;
    }
}
