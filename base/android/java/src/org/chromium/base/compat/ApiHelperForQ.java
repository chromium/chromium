// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.telephony.CellInfo;
import android.telephony.TelephonyManager;

import org.chromium.base.Callback;
import org.chromium.base.annotations.VerifiesOnQ;
import org.chromium.base.task.AsyncTask;

import java.util.List;
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
}
