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

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.services.IMetricsUploadService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/**
 * A custom WebView AndroidMetricsLogUploader. It
 * sends metrics logs to the nonembedded {@link
 * org.chromium.android_webview.services.MetricsUploadService} which then uploads them accordingly
 * depending on the platform implementation.
 */
public class AwMetricsLogUploader implements Consumer<byte[]> {
    private static final String TAG = "AwMetricsLogUploader";

    private final InitialMetricsServiceConnection mInitialConnection;

    public AwMetricsLogUploader() {
        mInitialConnection = new InitialMetricsServiceConnection();
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
                Log.w(TAG, "Failed to intially bind to MetricsUploadService");
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
        private final @NonNull byte[] mData;

        public MetricsLogUploaderServiceConnection(@NonNull byte[] data) {
            mData = data;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // onServiceConnected is called on the app main looper so post it to a background thread
            // for execution. No need to enforce the order in which the logs are sent to the service
            // as this isn't required/enforced by UMA.
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                IMetricsUploadService uploadService =
                        IMetricsUploadService.Stub.asInterface(service);
                try {
                    uploadService.uploadMetricsLog(mData);
                } catch (RemoteException e) {
                    Log.d(TAG, "Failed to send serialized metrics data to service", e);
                } finally {
                    ContextUtils.getApplicationContext().unbindService(this);
                }
            });
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}

        public void sendData() {
            bindToMetricsUploadService(this);
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
    public void accept(@NonNull byte[] data) {
        (new MetricsLogUploaderServiceConnection(data)).sendData();
        // Unbind the initial connection if it's still bound, since a new connection is now bound to
        // the service.
        mInitialConnection.unbind();
    }

    /**
     * Initialize a connection to {@link org.chromium.android_webview.services.MetricsUploadService}
     * and keep it alive until the first metrics log data is sent for upload.
     */
    public void initialize() {
        mInitialConnection.initialize();
    }
}
