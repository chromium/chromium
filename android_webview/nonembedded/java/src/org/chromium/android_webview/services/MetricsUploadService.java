// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.services.IMetricsUploadService;
import org.chromium.android_webview.services.ServicesStatsHelper.NonembeddedService;

/**
 * Service that receives UMA metrics logs from embedded WebView instances and send them to GMS-core
 * as is.
 */
public class MetricsUploadService extends Service {
    private static final String TAG = "MetricsUploadService";

    private final IMetricsUploadService.Stub mBinder = new IMetricsUploadService.Stub() {
        @Override
        public void uploadMetricsLog(byte[] serializedLog) {
            // TODO(crbug.com/1264425): return the status code.
            PlatformServiceBridge.getInstance().logMetricsBlocking(serializedLog);
        }
    };

    @Override
    public void onCreate() {
        ServicesStatsHelper.recordServiceLaunch(NonembeddedService.METRICS_UPLOAD_SERVICE);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
