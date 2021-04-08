// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Notification;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.hardware.biometrics.BiometricManager;
import android.net.NetworkCapabilities;
import android.net.TransportInfo;
import android.net.Uri;
import android.os.Build;
import android.os.FileUtils;
import android.provider.MediaStore;
import android.telephony.CellInfo;
import android.telephony.TelephonyManager;
import android.view.MotionEvent;

import org.chromium.base.Callback;
import org.chromium.base.annotations.VerifiesOnQ;
import org.chromium.base.task.AsyncTask;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * Utility class to use new APIs that were added in Q (API level 29). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
public final class ApiHelperForQ {
    private ApiHelperForQ() {}

    /** See {@link TelephonyManager.requestCellInfoUpdate() }. */
    public static void requestCellInfoUpdate(
            TelephonyManager telephonyManager, Callback<List<CellInfo>> callback) {
        telephonyManager.requestCellInfoUpdate(
                AsyncTask.THREAD_POOL_EXECUTOR, new TelephonyManager.CellInfoCallback() {
                    @Override
                    @SuppressLint("Override")
                    public void onCellInfo(List<CellInfo> cellInfos) {
                        callback.onResult(cellInfos);
                    }
                });
    }

    public static boolean bindIsolatedService(Context context, Intent intent, int flags,
            String instanceName, Executor executor, ServiceConnection connection) {
        return context.bindIsolatedService(intent, flags, instanceName, executor, connection);
    }

    public static void updateServiceGroup(
            Context context, ServiceConnection connection, int group, int importance) {
        context.updateServiceGroup(connection, group, importance);
    }

    /** See {@link MotionEvent#getClassification() }. */
    public static int getClassification(MotionEvent event) {
        return event.getClassification();
    }

    /** See {@link Context#getSystemService(Class<T>) }. */
    public static BiometricManager getBiometricManagerSystemService(Context context) {
        return context.getSystemService(BiometricManager.class);
    }

    /** See {@link Service#startForegroung(int, Notification, int) }. */
    public static void startForeground(
            Service service, int id, Notification notification, int foregroundServiceType) {
        service.startForeground(id, notification, foregroundServiceType);
    }

    /** See {@link FileUtils#copy(InputStream, OutputStream) }. */
    public static long copy(InputStream in, OutputStream out) throws IOException {
        return FileUtils.copy(in, out);
    }

    /** See {@link MediaStore#setIncludePending(Uri) }. */
    public static Uri setIncludePending(Uri uri) {
        return MediaStore.setIncludePending(uri);
    }

    /** See {@link MediaStore#getExternalVolumeNames(Context) }. */
    public static Set<String> getExternalVolumeNames(Context context) {
        return MediaStore.getExternalVolumeNames(context);
    }

    /** See {@link BiometricManager#canAuthenticate() }. */
    public static int canAuthenticate(BiometricManager manager) {
        return manager.canAuthenticate();
    }

    /** See {@link NetworkCapabilities#getTransportInfo() } */
    public static TransportInfo getTransportInfo(NetworkCapabilities networkCapabilities) {
        return networkCapabilities.getTransportInfo();
    }
}
