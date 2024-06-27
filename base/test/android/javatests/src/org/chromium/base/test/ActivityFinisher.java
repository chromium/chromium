// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.Application.ActivityLifecycleCallbacks;
import android.content.Context;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.chromium.base.ActivityLifecycleCallbacksAdapter;
import org.chromium.base.ActivityState;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Helpers for finishing activities and waiting for them to be destroyed.
 *
 * <p>AndroidX's test runner supports finishing activities, but does not support timeouts.
 *
 * <p>As of March 2024, finishAll() took ~250ms per-Activity on a P920 running an x86
 * Android O emulator.
 */
public class ActivityFinisher {
    private static final String TAG = "ActivityFinisher";

    public static List<Activity> snapshotActivities(
            @Nullable Collection<Activity> ignoreActivities) {
        List<Activity> ret = new ArrayList<>();
        // Cannot use ThreadUtils since WebView may override UI thread.
        Runnable r =
                () -> {
                    // AndroidX's ActivityFinisher also collects Activities in this way.
                    var lifecycleMonitor = ActivityLifecycleMonitorRegistry.getInstance();
                    for (Stage s : EnumSet.range(Stage.PRE_ON_CREATE, Stage.RESTARTED)) {
                        ret.addAll(lifecycleMonitor.getActivitiesInStage(s));
                    }
                };
        if (Looper.myLooper() == Looper.getMainLooper()) {
            r.run();
        } else {
            BaseChromiumAndroidJUnitRunner.sInstance.runOnMainSync(r);
        }
        if (ignoreActivities != null) {
            ret.removeAll(ignoreActivities);
        }
        return ret;
    }

    public static List<Activity> snapshotActivities() {
        return snapshotActivities(null);
    }

    /** Finishes all activities via AppTask.finishAndRemoveTask(). */
    public static void finishAll() {
        assert Looper.myLooper() != Looper.getMainLooper();
        UptimeMillisTimer timer = new UptimeMillisTimer();

        ActivityManager activityManager =
                (ActivityManager)
                        BaseChromiumAndroidJUnitRunner.sApplication.getSystemService(
                                Context.ACTIVITY_SERVICE);
        try {
            // Use multiple rounds in case new activities are started.
            int numTries = 5;
            for (int attempt = 0; attempt < numTries + 1; ++attempt) {
                if (attempt == numTries) {
                    Log.e(
                            TAG,
                            "Giving up after %d attempts. These still remain: %s",
                            attempt,
                            snapshotActivities());
                    break;
                }
                if (!finishHelper(activityManager)) {
                    if (attempt > 0) {
                        Log.i(
                                TAG,
                                "Finishing activities took %dms and %d iterations",
                                timer.getElapsedMillis(),
                                attempt);
                    }
                    break;
                }
            }
        } catch (TimeoutException e) {
            // The exception is logged in finishHelper();
        }
    }

    /** Returns whether any work was done. */
    private static boolean finishHelper(ActivityManager activityManager) throws TimeoutException {
        CallbackHelper doneCallback = new CallbackHelper();
        Set<Activity> remaining = Collections.synchronizedSet(new HashSet<>());
        AtomicBoolean didWorkHolder = new AtomicBoolean();
        ActivityLifecycleCallbacks lifecycleCallbacks =
                new ActivityLifecycleCallbacksAdapter() {
                    @Override
                    public void onStateChanged(Activity activity, @ActivityState int newState) {
                        // We are not guaranteed to have more than one activity be finished, so wait
                        // for only the first one.
                        if (newState == ActivityState.DESTROYED && remaining.contains(activity)) {
                            doneCallback.notifyCalled();
                        }
                    }
                };

        // Cannot use ThreadUtils since WebView may override UI thread.
        BaseChromiumAndroidJUnitRunner.sInstance.runOnMainSync(
                () -> {
                    // Collect activities on the UI thread to ensure that the list of
                    // activities do not change before installing the lifecycle listener.
                    List<AppTask> tasks = activityManager.getAppTasks();
                    List<Activity> activities = snapshotActivities();
                    if (!tasks.isEmpty() || !activities.isEmpty()) {
                        Log.i(
                                TAG,
                                "Finishing %d leftover tasks and these activities: %s",
                                tasks.size(),
                                activities);
                    }
                    // It's possible to have tasks but no activities when the test starts.
                    for (ActivityManager.AppTask task : tasks) {
                        try {
                            task.finishAndRemoveTask();
                            didWorkHolder.set(true);
                        } catch (Throwable t) {
                            Log.w(TAG, "Ignoring exception:", t);
                            // IllegalArgumentException when tasks disappear between querying
                            // the list of them and calling finish on them.
                            // http://crbug.com/343294387.
                        }
                    }
                    if (!activities.isEmpty()) {
                        // Even if we don't actually call .finish(), we still need to wait for
                        // already-finishing activities to be destroyed.
                        didWorkHolder.set(true);
                        for (Activity activity : activities) {
                            if (!activity.isFinishing()) {
                                activity.finishAndRemoveTask();
                            }
                        }
                    }

                    if (activities.isEmpty()) {
                        doneCallback.notifyCalled();
                    } else {
                        remaining.addAll(activities);
                        BaseChromiumAndroidJUnitRunner.sApplication
                                .registerActivityLifecycleCallbacks(lifecycleCallbacks);
                    }
                });
        if (!didWorkHolder.get()) {
            return false;
        }

        try {
            doneCallback.waitForNext();
            return true;
        } catch (TimeoutException e) {
            Log.w(TAG, "Timed out trying to close leftover activities: %s", remaining);
            throw e;
        } finally {
            // This call is thread-safe.
            BaseChromiumAndroidJUnitRunner.sApplication.unregisterActivityLifecycleCallbacks(
                    lifecycleCallbacks);
        }
    }
}
