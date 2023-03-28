// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.services.IMetricsUploadService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.metrics.AndroidMetricsLogConsumer;

import java.net.HttpURLConnection;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A custom WebView AndroidMetricsLogConsumer. It
 * sends metrics logs to the nonembedded {@link
 * org.chromium.android_webview.services.MetricsUploadService} which then uploads them accordingly
 * depending on the platform implementation.
 */
public class AwMetricsLogUploader implements AndroidMetricsLogConsumer {
    private static final String TAG = "AwMetricsLogUploader";
    private static final long SEND_DATA_TIMEOUT_MS = 10_000;

    private final InitialMetricsServiceConnection mInitialConnection;
    private final boolean mWaitForResults;
    private final boolean mUseDefaultUploadQos;

    /**
     * @param waitForResults Whether logging should wait for a status for the platform or return
     * early.
     */
    public AwMetricsLogUploader(boolean waitForResults, boolean useDefaultUploadQos) {
        mInitialConnection = new InitialMetricsServiceConnection();
        mWaitForResults = waitForResults;
        mUseDefaultUploadQos = useDefaultUploadQos;
    }

    // A service connection that is used to establish an initial connection to the
    // MetricsUploadService to keep it alive until the first metrics log is ready. Currently it does
    // nothing but it can be later used to query metrics service configs like sampling state of the
    // device ... etc, during startup.
    private static class InitialMetricsServiceConnection implements ServiceConnection {
        private final AtomicBoolean mBound = new AtomicBoolean();

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {}

        @Override
        public void onServiceDisconnected(ComponentName name) {}

        public void initialize() {
            boolean bindingResult = bindToMetricsUploadService(this);
            mBound.set(bindingResult);
            if (!bindingResult) {
                Log.w(TAG, "Failed to initially bind to MetricsUploadService");
            }
        }

        /**
         * Unbind the service connection if it's still bound to the service, do nothing otherwise.
         *
         * This method is thread-safe.
         */
        public void unbind() {
            if (mBound.getAndSet(false)) {
                ContextUtils.getApplicationContext().unbindService(this);
            }
        }
    }

    // A service connection that sends the given serialized metrics log data to
    // MetricsUploadService. It closes the connection after sending the metrics log.
    private static class MetricsLogUploaderServiceConnection implements ServiceConnection {
        private final boolean mUseDefaultUploadQos;
        private final @NonNull byte[] mData;
        private final CompletableFuture<Integer> mResult;
        private final AtomicBoolean mPosted = new AtomicBoolean();

        public MetricsLogUploaderServiceConnection(boolean useDefaultUploadQos,
                @NonNull byte[] data, @NonNull CompletableFuture<Integer> resultFuture) {
            mUseDefaultUploadQos = useDefaultUploadQos;
            mData = data;
            mResult = resultFuture;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // We want to avoid re-posting if the service connection dies
            // and reconnects.
            if (mPosted.getAndSet(true)) {
                return;
            }
            // onServiceConnected is called on the app main looper so post it to a background thread
            // for execution. No need to enforce the order in which the logs are sent to the service
            // as this isn't required/enforced by UMA.
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                IMetricsUploadService uploadService =
                        IMetricsUploadService.Stub.asInterface(service);
                try {
                    int status = uploadService.uploadMetricsLog(mData, mUseDefaultUploadQos);
                    mResult.complete(status);
                } catch (RemoteException e) {
                    Log.d(TAG, "Failed to send serialized metrics data to service", e);
                    mResult.complete(HttpURLConnection.HTTP_INTERNAL_ERROR);
                } finally {
                    ContextUtils.getApplicationContext().unbindService(this);
                }
            });
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}

        public int sendData(boolean waitForResults) {
            bindToMetricsUploadService(this);
            if (!waitForResults) {
                return HttpURLConnection.HTTP_OK;
            }
            // The logs could have still been sent by the service even if we haven't gotten a
            // response here so our choice is to either drop the logs or allow for duplication.
            try {
                return mResult.get(SEND_DATA_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            } catch (CancellationException e) {
                Log.e(TAG, "Request to send data cancelled", e);
                // If the future was cancelled, we will treat this as a situation to retry.
                return HttpURLConnection.HTTP_GONE;
            } catch (InterruptedException e) {
                Log.e(TAG, "Request to send data interrupted while waiting", e);
                return HttpURLConnection.HTTP_UNAVAILABLE;
            } catch (ExecutionException e) {
                Log.e(TAG, "Request to send data completed with exception", e);
                // In this case the request hit the server so we will treat this as a discarded log.
                // The Chromium metrics service will drop http 400s.
                return HttpURLConnection.HTTP_BAD_REQUEST;
            } catch (TimeoutException e) {
                Log.e(TAG, "Failed to receive response from upload service in time", e);
                // We decided to allow for duplication because there is quite a long time out so not
                // receiving a response for this long probably means there was an issue with
                // logging. This code path could also be called if the service connection has a
                // failure.
                return HttpURLConnection.HTTP_CLIENT_TIMEOUT;
            }
        }
    }

    private static boolean bindToMetricsUploadService(ServiceConnection connection) {
        Intent intent = new Intent();
        intent.setClassName(
                AwBrowserProcess.getWebViewPackageName(), ServiceNames.METRICS_UPLOAD_SERVICE);
        return ServiceHelper.bindService(
                ContextUtils.getApplicationContext(), intent, connection, Context.BIND_AUTO_CREATE);
    }

    /**
     * Send the log to the upload service.
     *
     * @param data serialized ChromeUserMetricsExtension proto message.
     */
    @Override
    public int log(@NonNull byte[] data) {
        return log(data, new CompletableFuture());
    }

    @VisibleForTesting
    public int log(@NonNull byte[] data, @NonNull CompletableFuture<Integer> resultFuture) {
        MetricsLogUploaderServiceConnection connection =
                new MetricsLogUploaderServiceConnection(mUseDefaultUploadQos, data, resultFuture);
        int status = connection.sendData(mWaitForResults);
        // Unbind the initial connection if it's still bound, since a new connection is now bound to
        // the service.
        mInitialConnection.unbind();
        return status;
    }

    /**
     * Initialize a connection to {@link org.chromium.android_webview.services.MetricsUploadService}
     * and keep it alive until the first metrics log data is sent for upload.
     */
    public void initialize() {
        mInitialConnection.initialize();
    }
}
