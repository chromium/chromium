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
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.services.IMetricsUploadService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.metrics.AndroidMetricsLogConsumer;

import java.net.HttpURLConnection;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A custom WebView AndroidMetricsLogConsumer. It
 * sends metrics logs to the nonembedded {@link
 * org.chromium.android_webview.services.MetricsUploadService} which then uploads them accordingly
 * depending on the platform implementation.
 */
@Lifetime.Singleton
public class AwMetricsLogUploader implements AndroidMetricsLogConsumer {
    private static final String TAG = "AwMetricsLogUploader";
    private static final long SERVICE_CONNECTION_TIMEOUT_MS = 10_000;

    private final AtomicReference<MetricsLogUploaderServiceConnection> mInitialConnection;
    private final boolean mIsAsync;

    /**
     * @param isAsync Whether logging is happening on a background thread or if it is being called
     *     from the main thread.
     */
    public AwMetricsLogUploader(boolean isAsync) {
        // A service connection that is used to establish an initial connection to the
        // MetricsUploadService to keep it alive until the first metrics log is ready.
        mInitialConnection = new AtomicReference();
        mIsAsync = isAsync;
    }

    // A service connection that sends the given serialized metrics log data to
    // MetricsUploadService. It closes the connection after sending the metrics log.
    private static class MetricsLogUploaderServiceConnection implements ServiceConnection {
        private final LinkedBlockingQueue<IMetricsUploadService> mConnectionsQueue;

        public MetricsLogUploaderServiceConnection(
                LinkedBlockingQueue<IMetricsUploadService> connectionsQueue) {
            mConnectionsQueue = connectionsQueue;
        }

        public boolean bind() {
            Intent intent = new Intent();
            intent.setClassName(
                    AwBrowserProcess.getWebViewPackageName(), ServiceNames.METRICS_UPLOAD_SERVICE);
            return ServiceHelper.bindService(
                    ContextUtils.getApplicationContext(), intent, this, Context.BIND_AUTO_CREATE);
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // If onServiceConnected is incorrectly called twice in a row without
            // onServiceDisconnected, we will still try take the latest service connection for
            // a hope of working.
            mConnectionsQueue.clear();
            IMetricsUploadService uploadService = IMetricsUploadService.Stub.asInterface(service);
            // Keep track of if the service is updated again since the last clear.
            if (!mConnectionsQueue.offer(uploadService)) {
                Log.d(TAG, "Attempted to re-bind with service twice.");
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            // If we get an unexpected disconnection, we should no longer trust the connection we
            // have queued.
            // If the metrics service already has a connection it will simply fail when trying to
            // make a call.
            // This should be helpful for the first connection where there is a more considerable
            // delta between when we first bind, and when we try to send data.
            mConnectionsQueue.clear();
        }

        /**
         * Note: Once this method has run, it will automatically unbind the connection so this
         * connection should not be used after calling this method "once".
         */
        public int sendData(boolean isAsync, @NonNull byte[] data) {
            // If we are on the main thread, we cannot block waiting to connect to the service so we
            // need to fire and forget. In this case all we can do is report back OK.
            if (!isAsync) {
                PostTask.postTask(
                        TaskTraits.BEST_EFFORT_MAY_BLOCK,
                        () -> {
                            uploadToService(data);
                        });

                return HttpURLConnection.HTTP_OK;
            }

            return uploadToService(data);
        }

        private int uploadToService(@NonNull byte[] data) {
            try {
                IMetricsUploadService uploadService =
                        mConnectionsQueue.poll(
                                SERVICE_CONNECTION_TIMEOUT_MS, TimeUnit.MILLISECONDS);

                // Null returned from poll means we timed out
                if (uploadService == null) {
                    Log.e(TAG, "Failed to receive response from upload service in time");
                    return HttpURLConnection.HTTP_CLIENT_TIMEOUT;
                }

                return uploadService.uploadMetricsLog(data);
            } catch (RemoteException e) {
                Log.d(TAG, "Failed to send serialized metrics data to service", e);
            } catch (InterruptedException e) {
                Log.e(TAG, "Request to send data interrupted while waiting", e);
                return HttpURLConnection.HTTP_UNAVAILABLE;
            } finally {
                ContextUtils.getApplicationContext().unbindService(this);
            }

            return HttpURLConnection.HTTP_INTERNAL_ERROR;
        }
    }

    /**
     * Send the log to the upload service.
     *
     * @param data serialized ChromeUserMetricsExtension proto message.
     */
    @Override
    public int log(@NonNull byte[] data) {
        return log(data, new LinkedBlockingQueue(1));
    }

    @VisibleForTesting
    public int log(
            @NonNull byte[] data,
            @NonNull LinkedBlockingQueue<IMetricsUploadService> connectionsQueue) {
        MetricsLogUploaderServiceConnection connection = mInitialConnection.getAndSet(null);

        if (connection == null) {
            connection = new MetricsLogUploaderServiceConnection(connectionsQueue);

            if (!connection.bind()) {
                Log.w(TAG, "Failed to bind to MetricsUploadService");
                return HttpURLConnection.HTTP_UNAVAILABLE;
            }
        }

        return connection.sendData(mIsAsync, data);
    }

    /**
     * Initialize a connection to {@link org.chromium.android_webview.services.MetricsUploadService}
     * and keep it alive until the first metrics log data is sent for upload.
     *
     * <p>We do this because we already pay the startup cost of the non-embedded process due to
     * other webview non-embedded services running early on. We can hopefully save some time on
     * initially spinning the process since we know we are going to attempt to upload pretty soon
     * after starting up WebView the first time.
     */
    public void initialize() {
        MetricsLogUploaderServiceConnection connection =
                new MetricsLogUploaderServiceConnection(new LinkedBlockingQueue(1));
        if (connection.bind()) {
            mInitialConnection.set(connection);
        } else {
            Log.w(TAG, "Failed to initially bind to MetricsUploadService");
        }
    }
}
