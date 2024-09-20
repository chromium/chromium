// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.services.IMetricsUploadService;

import java.net.HttpURLConnection;

/**
 * Service that receives UMA metrics logs from embedded WebView instances and send them to GMS-core
 * as is.
 */
public class MetricsUploadService extends Service {
    private final IMetricsUploadService.Stub mBinder =
            new IMetricsUploadService.Stub() {
                @Override
                public int uploadMetricsLog(byte[] serializedLog) {
                    int status =
                            PlatformServiceBridge.getInstance().logMetricsBlocking(serializedLog);

                    // We map the platform reported statuses out to HTTP status codes so that
                    // Chromium metrics can work with a single standard.
                    switch (status) {
                            // The platform will return -1 or 0 as a success status.
                            // The Chromium metrics work with status codes in HTTP status codes so
                            // we will convert these over.
                        case -1:
                        case 0:
                            return HttpURLConnection.HTTP_OK;
                            // This is essentially the same as receiving an http 500.
                            // All we know is something went wrong internally.
                        case 8:
                            return HttpURLConnection.HTTP_INTERNAL_ERROR;
                            // The request was interrupted so we're treating this the same way we
                            // treat an interruption in {@link
                            // org.chromium.android_webview.metrics.AwMetricsLogUploader}
                        case 14:
                            return HttpURLConnection.HTTP_UNAVAILABLE;
                            // The request timed out so we should try again.
                        case 15:
                            return HttpURLConnection.HTTP_GATEWAY_TIMEOUT;
                            // The request was cancelled to the client API.
                        case 16:
                            return HttpURLConnection.HTTP_GONE;
                            // The API is not available on the device so we should ensure this isn't
                            // called again.
                        case 17:
                            return HttpURLConnection.HTTP_BAD_REQUEST;
                            // If we receive a non expected status from the platform, we will report
                            // this as bad to be rejected to the chromium metrics.
                        default:
                            return HttpURLConnection.HTTP_BAD_REQUEST;
                    }
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
