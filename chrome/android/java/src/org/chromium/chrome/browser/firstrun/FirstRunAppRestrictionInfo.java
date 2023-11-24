// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.AppRestrictionsProvider;
import org.chromium.components.policy.PolicySwitches;

import java.util.LinkedList;
import java.util.Locale;
import java.util.Queue;
import java.util.concurrent.RejectedExecutionException;

/**
 * A helper class used to check if app restrictions are present during first run. Internally it
 * uses an asynchronous background task to fetch restrictions, then notifies registered callbacks
 * once complete.
 *
 * In order to get a result as soon as possible, this class provides a mechanism to kick off the
 * async via {@link #startInitializationHint()}. This instance will be stored in a private static
 * field, and will be returned by {@link #takeMaybeInitialized}, as well as nulling out the private
 * static. This mechanism doesn't strictly need to be used, and {@link #startInitializationHint()}
 * can be ignored at the cost of latency.
 *
 * This class is only used during first run flow, so its lifecycle ends when first run completes. At
 * which point {@link #destroy()} should be called.
 */
class FirstRunAppRestrictionInfo {
    private static final String TAG = "FRAppRestrictionInfo";

    private static FirstRunAppRestrictionInfo sInitializedInstance;

    private boolean mInitialized;
    private boolean mHasAppRestriction;
    private long mCompletionElapsedRealtimeMs;
    private Queue<Callback<Boolean>> mCallbacks = new LinkedList<>();
    private Queue<Callback<Long>> mCompletionTimeCallbacks = new LinkedList<>();

    private AsyncTask<Boolean> mFetchAppRestrictionAsyncTask;

    private FirstRunAppRestrictionInfo() {
        initialize();
    }

    /**
     * Starts initialization and stores an instance of {@link FirstRunAppRestrictionInfo} in a
     * static field. This will be waiting for the first caller of
     * {@link FirstRunAppRestrictionInfo#takeMaybeInitialized()}.
     */
    public static void startInitializationHint() {
        if (sInitializedInstance == null) {
            sInitializedInstance = new FirstRunAppRestrictionInfo();
        }
    }

    /**
     * Tries to transfer ownership of the previously instantiated static instance if possible. When
     * there is no such instance, this will simply return a new {@link FirstRunAppRestrictionInfo}.
     * Either way, an async check for app restrictions will have been started before this method
     * returns. Call {@link #getHasAppRestriction(Callback)} to be notified when the check
     * completes.
     */
    public static FirstRunAppRestrictionInfo takeMaybeInitialized() {
        ThreadUtils.assertOnUiThread();
        FirstRunAppRestrictionInfo info;
        if (sInitializedInstance == null) {
            info = new FirstRunAppRestrictionInfo();
        } else {
            info = sInitializedInstance;
            sInitializedInstance = null;
        }
        return info;
    }

    /** Stops the async initialization if it is in progress, and remove all the callbacks. */
    public void destroy() {
        if (mFetchAppRestrictionAsyncTask != null) {
            mFetchAppRestrictionAsyncTask.cancel(true);
        }
        mCallbacks.clear();
        mCompletionTimeCallbacks.clear();
    }

    /**
     * Register a callback whether app restriction is found on device. If app restrictions have
     * already been fetched, the callback will be invoked immediately.
     *
     * @param callback Callback to run with whether app restriction is found on device.
     */
    public void getHasAppRestriction(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();

        if (mInitialized) {
            callback.onResult(mHasAppRestriction);
        } else {
            mCallbacks.add(callback);
        }
    }

    /**
     * Registers a callback for the timestamp from {@link SystemClock#elapsedRealtime} when the app
     * restrictions call finished. If the restrictions have already been fetched, the callback will
     * be invoked immediately.
     *
     * @param callback Callback to run with the timestamp of completing the fetch.
     */
    public void getCompletionElapsedRealtimeMs(Callback<Long> callback) {
        ThreadUtils.assertOnUiThread();
        if (mInitialized) {
            callback.onResult(mCompletionElapsedRealtimeMs);
        } else {
            mCompletionTimeCallbacks.add(callback);
        }
    }

    /** Start fetching app restriction on an async thread. */
    private void initialize() {
        ThreadUtils.assertOnUiThread();
        long startTime = SystemClock.elapsedRealtime();

        // This is an imperfect system, and can sometimes return true when there will not actually
        // be any app restrictions. But we do not have parsing logic in Java to understand if the
        // switch sets valid policies.
        if (CommandLine.getInstance().hasSwitch(PolicySwitches.CHROME_POLICY)) {
            onRestrictionDetected(true, startTime);
            return;
        }

        if (AbstractAppRestrictionsProvider.hasTestRestrictions()) {
            onRestrictionDetected(true, startTime);
            return;
        }

        Context appContext = ContextUtils.getApplicationContext();
        try {
            mFetchAppRestrictionAsyncTask =
                    new AsyncTask<Boolean>() {
                        @Override
                        protected Boolean doInBackground() {
                            UserManager userManager =
                                    (UserManager) appContext.getSystemService(Context.USER_SERVICE);
                            Bundle bundle =
                                    AppRestrictionsProvider
                                            .getApplicationRestrictionsFromUserManager(
                                                    userManager, appContext.getPackageName());
                            return bundle != null && !bundle.isEmpty();
                        }

                        @Override
                        protected void onPostExecute(Boolean isAppRestricted) {
                            onRestrictionDetected(isAppRestricted, startTime);
                        }
                    };
            mFetchAppRestrictionAsyncTask.executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
        } catch (RejectedExecutionException e) {
            // Though unlikely, if the task is rejected, we assume no restriction exists.
            onRestrictionDetected(false, startTime);
        }
    }

    private void onRestrictionDetected(boolean isAppRestricted, long startTime) {
        mHasAppRestriction = isAppRestricted;
        mInitialized = true;

        // Only record histogram when startTime is valid.
        if (startTime > 0) {
            mCompletionElapsedRealtimeMs = SystemClock.elapsedRealtime();
            long runTime = mCompletionElapsedRealtimeMs - startTime;
            Log.i(
                    TAG,
                    String.format(
                            Locale.US,
                            "Policy received. Runtime: [%d], result: [%s]",
                            runTime,
                            isAppRestricted));
        }

        while (!mCallbacks.isEmpty()) {
            mCallbacks.remove().onResult(mHasAppRestriction);
        }
        while (!mCompletionTimeCallbacks.isEmpty()) {
            mCompletionTimeCallbacks.remove().onResult(mCompletionElapsedRealtimeMs);
        }
    }

    static void setInitializedInstanceForTest(
            FirstRunAppRestrictionInfo firstRunAppRestrictionInfo) {
        var oldValue = sInitializedInstance;
        sInitializedInstance = firstRunAppRestrictionInfo;
        ResettersForTesting.register(() -> sInitializedInstance = oldValue);
    }
}
