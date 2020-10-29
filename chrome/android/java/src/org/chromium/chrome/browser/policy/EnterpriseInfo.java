// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeSwitches;

import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.RejectedExecutionException;

/**
 * Provide the enterprise information for the current device and profile.
 */
public class EnterpriseInfo {
    private static final String TAG = "EnterpriseInfo";
    private final Handler mHandler;

    private static EnterpriseInfo sInstance;

    // Only ever read/written on the UI thread.
    private OwnedState mOwnedState;
    private Queue<Callback<OwnedState>> mCallbackList;

    private boolean mSkipAsyncCheckForTesting;

    /** A simple tuple to hold onto named fields about the state of ownership. */
    public static class OwnedState {
        public final boolean mDeviceOwned;
        public final boolean mProfileOwned;

        public OwnedState(boolean isDeviceOwned, boolean isProfileOwned) {
            mDeviceOwned = isDeviceOwned;
            mProfileOwned = isProfileOwned;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (!(other instanceof OwnedState)) return false;

            OwnedState otherOwnedState = (OwnedState) other;

            return this.mDeviceOwned == otherOwnedState.mDeviceOwned
                    && this.mProfileOwned == otherOwnedState.mProfileOwned;
        }
    }

    public static EnterpriseInfo getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) sInstance = new EnterpriseInfo();

        return sInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTest(EnterpriseInfo instance) {
        sInstance = instance;
    }

    /**
     * Returns, via callback, whether the device has a device owner or a profile owner.
     */
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
                    long startTime = SystemClock.elapsedRealtime();
                    boolean hasProfileOwnerApp = false;
                    boolean hasDeviceOwnerApp = false;
                    PackageManager packageManager = context.getPackageManager();
                    DevicePolicyManager devicePolicyManager =
                            (DevicePolicyManager) context.getSystemService(
                                    Context.DEVICE_POLICY_SERVICE);

                    if (CommandLine.getInstance().hasSwitch(
                                ChromeSwitches.FORCE_DEVICE_OWNERSHIP)) {
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

                    long endTime = SystemClock.elapsedRealtime();
                    RecordHistogram.recordTimesHistogram(
                            "EnterpriseCheck.IsRunningOnManagedProfileDuration",
                            endTime - startTime);

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
            mHandler.post(() -> { failedRunCallback.onResult(null); });
        }
    }

    /**
     * Records metrics regarding whether the device has a device owner or a profile owner.
     */
    public void logDeviceEnterpriseInfo() {
        Callback<OwnedState> callback = (result) -> {
            recordManagementHistograms(result);
        };

        getDeviceEnterpriseInfo(callback);
    }

    private EnterpriseInfo() {
        mOwnedState = null;
        mCallbackList = new LinkedList<Callback<OwnedState>>();
        mHandler = new Handler(Looper.myLooper());
    }

    @VisibleForTesting
    void setCacheResult(OwnedState result) {
        ThreadUtils.assertOnUiThread();
        assert result != null;
        mOwnedState = result;
    }

    @VisibleForTesting
    void onEnterpriseInfoResultAvailable() {
        ThreadUtils.assertOnUiThread();
        assert mOwnedState != null;

        // Service every waiting callback.
        while (mCallbackList.size() > 0) mCallbackList.remove().onResult(mOwnedState);
    }

    private void recordManagementHistograms(OwnedState state) {
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
    @VisibleForTesting
    void setSkipAsyncCheckForTesting(boolean skip) {
        mSkipAsyncCheckForTesting = skip;
    }

    @VisibleForTesting
    static void reset() {
        sInstance = null;
    }

    /**
     * Returns, via callback, the ownded state for native's AndroidEnterpriseInfo.
     */
    @CalledByNative
    public static void getManagedStateForNative() {
        Callback<OwnedState> callback = (result) -> {
            if (result == null) {
                // Unable to determine the owned state, assume it's not owned.
                EnterpriseInfoJni.get().updateNativeOwnedState(false, false);
            } else {
                EnterpriseInfoJni.get().updateNativeOwnedState(
                        result.mDeviceOwned, result.mProfileOwned);
            }
        };

        EnterpriseInfo.getInstance().getDeviceEnterpriseInfo(callback);
    }

    @NativeMethods
    interface Natives {
        void updateNativeOwnedState(boolean hasProfileOwnerApp, boolean hasDeviceOwnerApp);
    }
}
