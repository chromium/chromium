// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.common.services.IVariationsSeedServerCallback;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * VariationsSeedServer is a bound service that shares the Variations seed with all the WebViews
 * on the system. A WebView will bind and call getSeed, passing a file descriptor to which the
 * service should write the seed.
 */
public class VariationsSeedServer extends Service {
    private static final String TAG = "VariationsSeedServer";

    private final IVariationsSeedServer.Stub mBinder =
            new IVariationsSeedServer.Stub() {
                @Override
                public void getSeed(
                        ParcelFileDescriptor newSeedFile,
                        long oldSeedDate,
                        IVariationsSeedServerCallback callback) {
                    maybeReportMetrics(callback);
                    VariationsSeedHolder.getInstance().writeSeedIfNewer(newSeedFile, oldSeedDate);
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private void maybeReportMetrics(IVariationsSeedServerCallback callback) {
        Context context = ContextUtils.getApplicationContext();
        VariationsServiceMetricsHelper metrics =
                VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
        Bundle metricsBundle = metrics.toBundle();
        if (metricsBundle.isEmpty()) {
            return;
        }

        try {
            callback.reportVariationsServiceMetrics(metricsBundle);
        } catch (RemoteException e) {
            Log.e(TAG, "Error calling reportVariationsServiceMetrics", e);
            // If there was an error reporting the metrics, return so we don't clear them. The
            // next IPC will try again.
            return;
        }

        // Remove metrics from SharedPreferences once they've been reported so they won't get
        // reported a second time.
        metrics.clearJobInterval();
        metrics.clearJobQueueTime();
        if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
            Log.e(TAG, "Failed to write variations SharedPreferences to disk");
        }
    }
}
