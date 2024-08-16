// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.services.INetLogService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

// Connects to AwNetLogService service.
@JNINamespace("android_webview")
public class AwNetLogsConnection {
    private static final String TAG = "AwNetLogsConnection";

    // True if we should be logging. //
    private static boolean sLoggingEnabled;

    public static void startConnectNetLogService() {
        ThreadUtils.assertOnUiThread();
        sLoggingEnabled = true;
        final Context context = ContextUtils.getApplicationContext();
        final Intent intent = new Intent();
        intent.setClassName(AwBrowserProcess.getWebViewPackageName(), ServiceNames.NET_LOG_SERVICE);
        ServiceConnection connection =
                new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName className, IBinder service) {
                        ThreadUtils.runOnUiThread(
                                () -> {
                                    if (!sLoggingEnabled) {
                                        context.unbindService(this);
                                        return;
                                    }
                                    PostTask.postTask(
                                            TaskTraits.BEST_EFFORT,
                                            () -> {
                                                try {
                                                    String packageName = context.getPackageName();
                                                    final long creationTime =
                                                            System.currentTimeMillis();
                                                    INetLogService netLogService =
                                                            INetLogService.Stub.asInterface(
                                                                    service);
                                                    ParcelFileDescriptor parcelFileDescriptor =
                                                            netLogService.streamLog(
                                                                    creationTime, packageName);
                                                    if (parcelFileDescriptor != null) {
                                                        int fd = parcelFileDescriptor.detachFd();
                                                        AwNetLogsConnectionJni.get()
                                                                .startNetLogs(fd);
                                                    }
                                                } catch (RemoteException e) {
                                                    Log.e(
                                                            TAG,
                                                            "Failed to get ParcelFileDescriptor"
                                                                    + " from NetLogService",
                                                            e);
                                                    return;
                                                } finally {
                                                    context.unbindService(this);
                                                }
                                            });
                                });
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName className) {}
                };
        try {
            if (!ServiceHelper.bindService(context, intent, connection, Context.BIND_AUTO_CREATE)) {
                Log.w(TAG, "Could not bind to NetLogService " + intent);
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "Exception during stream net log process", e);
        }
    }

    public static void stopNetLogService() {
        ThreadUtils.assertOnUiThread();
        if (sLoggingEnabled) {
            AwNetLogsConnectionJni.get().stopNetLogs();
        }
        sLoggingEnabled = false;
    }

    @NativeMethods
    interface Natives {
        void startNetLogs(int pfd);

        void stopNetLogs();
    }
}
