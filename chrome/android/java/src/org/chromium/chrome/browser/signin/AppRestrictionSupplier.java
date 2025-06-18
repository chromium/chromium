// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.AppRestrictionsProvider;
import org.chromium.components.policy.PolicySwitches;

import java.util.Locale;
import java.util.concurrent.RejectedExecutionException;

/**
 * Checks if app restrictions are present during a fullscreen signin activity. Internally it uses an
 * asynchronous background task to fetch restrictions, then notifies registered callbacks once
 * complete.
 *
 * <p>This class is used during fullscreen signin flows, so its lifecycle ends when the flow
 * completes. At which point {@link #destroy()} should be called.
 */
@NullMarked
public class AppRestrictionSupplier implements OneshotSupplier<Boolean> {
    private static final String TAG = "AppRestriction";

    private long mCompletionElapsedRealtimeMs;
    private final OneshotSupplierImpl<Boolean> mSupplier = new OneshotSupplierImpl<>();

    private @Nullable AsyncTask<Boolean> mFetchAppRestrictionAsyncTask;

    public AppRestrictionSupplier() {
        ThreadUtils.assertOnUiThread();
        initialize();
    }

    @Override
    public @Nullable Boolean onAvailable(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        return mSupplier.onAvailable(callback);
    }

    @Override
    // TODO(https://github.com/uber/NullAway/issues/1209): Remove @SuppressWarnings("NullAway")
    // when the issue gets fixed
    @SuppressWarnings("NullAway")
    public @Nullable Boolean get() {
        return mSupplier.get();
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
                    new AsyncTask<>() {
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
        mSupplier.set(isAppRestricted);
    }
}
