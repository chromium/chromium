// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeSwitches;

import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.RejectedExecutionException;

/** The typical implementation of {@link EnterpriseInfo} at runtime. */
public class EnterpriseInfoImpl extends EnterpriseInfo {
    private static final String TAG = "EnterpriseInfoImpl";
    private final Handler mHandler;

    // Only ever read/written on the UI thread.
    private OwnedState mOwnedState;
    private Queue<Callback<OwnedState>> mCallbackList;

    private boolean mSkipAsyncCheckForTesting;

    EnterpriseInfoImpl() {
        mOwnedState = null;
        mCallbackList = new LinkedList<>();
        mHandler = new Handler(Looper.myLooper());
    }

    @Override
    @SuppressWarnings("QueryPermissionsNeeded")
    public void getDeviceEnterpriseInfo(Callback<OwnedState> callback) {
        // AsyncTask requires being called from UI thread.
        ThreadUtils.assertOnUiThread();
        assert callback != null;

        // If there is already a cached result post a task to return it.
        if (mOwnedState != null) {
            mHandler.post(() -> callback.onResult(mOwnedState));
            return;
        }

        // We need to make sure that nothing gets added to mCallbackList once there is a cached
        // result as nothing on this list will be ran again.
        mCallbackList.add(callback);

        if (mCallbackList.size() > 1) {
            // A pending callback is already being worked on, just add to the list and wait.
            return;
        }

        // Skip querying the device if we're testing.
        if (mSkipAsyncCheckForTesting) return;

        // There is no cached value and this is the first request, spin up a thread to query the
        // device.
        try {
            new AsyncTask<OwnedState>() {
                // TODO: Unit test this function. https://crbug.com/1099262
                private OwnedState calculateIsRunningOnManagedProfile(Context context) {
                    boolean hasProfileOwnerApp = false;
                    boolean hasDeviceOwnerApp = false;
                    PackageManager packageManager = context.getPackageManager();
                    DevicePolicyManager devicePolicyManager =
                            (DevicePolicyManager)
                                    context.getSystemService(Context.DEVICE_POLICY_SERVICE);

                    if (CommandLine.getInstance()
                            .hasSwitch(ChromeSwitches.FORCE_DEVICE_OWNERSHIP)) {
                        hasDeviceOwnerApp = true;
                    }

                    for (PackageInfo pkg : packageManager.getInstalledPackages(/* flags= */ 0)) {
                        assert devicePolicyManager != null;
                        if (devicePolicyManager.isProfileOwnerApp(pkg.packageName)) {
                            hasProfileOwnerApp = true;
                        }
                        if (devicePolicyManager.isDeviceOwnerApp(pkg.packageName)) {
                            hasDeviceOwnerApp = true;
                        }
                        if (hasProfileOwnerApp && hasDeviceOwnerApp) break;
                    }

                    return new OwnedState(hasDeviceOwnerApp, hasProfileOwnerApp);
                }

                @Override
                protected OwnedState doInBackground() {
                    Context context = ContextUtils.getApplicationContext();
                    return calculateIsRunningOnManagedProfile(context);
                }

                @Override
                protected void onPostExecute(OwnedState result) {
                    setCacheResult(result);
                    onEnterpriseInfoResultAvailable();
                }
            }.executeWithTaskTraits(TaskTraits.USER_VISIBLE);
        } catch (RejectedExecutionException e) {
            // This is an extreme edge case, but if it does happen then return null to indicate we
            // couldn't execute.
            Log.w(TAG, "Thread limit reached, unable to determine managed state.");

            // There will only ever be a single item in the queue as we only try()/catch() on the
            // first item.
            Callback<OwnedState> failedRunCallback = mCallbackList.remove();
            mHandler.post(() -> failedRunCallback.onResult(null));
        }
    }

    @VisibleForTesting
    void setCacheResult(OwnedState result) {
        ThreadUtils.assertOnUiThread();
        assert result != null;
        mOwnedState = result;
        Log.i(
                TAG,
                "#setCacheResult() deviceOwned:"
                        + result.mDeviceOwned
                        + " profileOwned:"
                        + result.mProfileOwned);
    }

    @VisibleForTesting
    void onEnterpriseInfoResultAvailable() {
        ThreadUtils.assertOnUiThread();
        assert mOwnedState != null;

        // Service every waiting callback.
        while (mCallbackList.size() > 0) mCallbackList.remove().onResult(mOwnedState);
    }

    @Override
    public void logDeviceEnterpriseInfo() {
        Callback<OwnedState> callback =
                (result) -> {
                    recordManagementHistograms(result);
                };
        getDeviceEnterpriseInfo(callback);
    }

    private static void recordManagementHistograms(OwnedState state) {
        if (state == null) return;

        RecordHistogram.recordBooleanHistogram("EnterpriseCheck.IsManaged2", state.mProfileOwned);
        RecordHistogram.recordBooleanHistogram(
                "EnterpriseCheck.IsFullyManaged2", state.mDeviceOwned);
    }

    /**
     * When true the check if a device/profile is managed is skipped, meaning that the callback
     * provided to getDeviceEnterpriseInfo is only added to mCallbackList. setCacheResult and
     * onEnterpriseInfoResultAvailable must be called manually.
     *
     * If mOwnedState != null then this function has no effect and a task to service the
     * callback will be posted immediately.
     */
    void setSkipAsyncCheckForTesting(boolean skip) {
        mSkipAsyncCheckForTesting = skip;
        ResettersForTesting.register(() -> mSkipAsyncCheckForTesting = false);
    }
}
