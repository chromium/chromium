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
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Helpers for finishing activities and waiting for them to be destroyed.
 *
 * <p>AndroidX's test runner supports finishing activities, but does not support timeouts and
 * leaving activities started in @BeforeClass.
 *
 * <p>As of March 2024, finishAll() took ~250ms per-Activity, and finishAllExceptFor() took ~300ms
 * per-Activity on a P920 running an x86 Android O emulator.
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
        List<AppTask> allTasks = activityManager.getAppTasks();
        if (allTasks.isEmpty()) {
            return;
        }
        try {
            // Use multiple rounds in case new activities are started.
            for (int attempts = 0; attempts < 3; ++attempts) {
                int numActivities = finishAllTasks(allTasks);
                allTasks = activityManager.getAppTasks();
                if (allTasks.isEmpty()) {
                    Log.i(
                            TAG,
                            "Closed all %d leftover activities in %s milliseconds.",
                            numActivities,
                            timer.getElapsedMillis());
                    return;
                }
                Log.i(
                        TAG,
                        "Closed %d leftover activities, but more %s tasks remain.",
                        numActivities,
                        allTasks.size());
            }
        } catch (TimeoutException e) {
            // The exception is logged in finishHelper();
        }
    }

    private static int finishAllTasks(List<AppTask> allTasks) throws TimeoutException {
        AtomicInteger numActivitiesHolder = new AtomicInteger();
        boolean succeeded =
                finishHelper(
                        () -> {
                            List<Activity> ret = snapshotActivities();
                            Log.i(TAG, "Finishing leftover Activities: %s", ret);
                            numActivitiesHolder.set(ret.size());
                            for (ActivityManager.AppTask task : allTasks) {
                                task.finishAndRemoveTask();
                            }
                            return ret;
                        });
        return succeeded ? numActivitiesHolder.get() : 0;
    }

    /** When ignoreActivities is present, finishes activities one by one via Activity.finish(). */
    public static void finishAllExceptFor(@Nullable Collection<Activity> ignoreActivities) {
        assert Looper.myLooper() != Looper.getMainLooper();
        // finishAll() is a bit faster.
        if (ignoreActivities == null || ignoreActivities.isEmpty()) {
            finishAll();
            return;
        }
        UptimeMillisTimer timer = new UptimeMillisTimer();
        try {
            // Use multiple rounds in case new activities are started, and to ensure deterministic
            // close order.
            for (int i = 0; i < 20; ++i) {
                if (!finishOneActivity(ignoreActivities)) {
                    if (i > 0) {
                        Log.i(
                                TAG,
                                "Closed %s leftover activities in %s milliseconds.",
                                i,
                                timer.getElapsedMillis());
                    }
                    return;
                }
            }
            Log.w(
                    TAG,
                    "More than 20 activities finished, but some still remain: %s",
                    snapshotActivities(ignoreActivities));
        } catch (TimeoutException e) {
            // The exception is logged in finishHelper();
        }
    }

    private static boolean finishOneActivity(Collection<Activity> ignoreActivities)
            throws TimeoutException {
        return finishHelper(
                () -> {
                    List<Activity> activities = snapshotActivities(ignoreActivities);
                    if (activities.isEmpty()) {
                        return Collections.emptyList();
                    }
                    // Finish the activity with the highest state.
                    Activity targetActivity = activities.get(activities.size() - 1);
                    if (!targetActivity.isFinishing()) {
                        targetActivity.finish();
                    }
                    Log.i(TAG, "Finishing leftover activity: %s", targetActivity);
                    return List.of(targetActivity);
                });
    }

    /** Returns whether all activities were destroyed. */
    private static boolean finishHelper(Supplier<List<Activity>> collectAndFinish)
            throws TimeoutException {
        CallbackHelper doneCallback = new CallbackHelper();
        Set<Activity> remaining = Collections.synchronizedSet(new HashSet<>());
        ActivityLifecycleCallbacks lifecycleCallbacks =
                new ActivityLifecycleCallbacksAdapter() {
                    @Override
                    public void onStateChanged(Activity activity, @ActivityState int newState) {
                        if (newState == ActivityState.DESTROYED && remaining.contains(activity)) {
                            remaining.remove(activity);
                            if (remaining.isEmpty()) {
                                doneCallback.notifyCalled();
                            }
                        }
                    }
                };

        // Cannot use ThreadUtils since WebView may override UI thread.
        BaseChromiumAndroidJUnitRunner.sInstance.runOnMainSync(
                () -> {
                    // Collect activities on the UI thread to ensure that the list of
                    // activities do not
                    // change before installing the lifecycle listener.
                    remaining.addAll(collectAndFinish.get());
                    if (!remaining.isEmpty()) {
                        BaseChromiumAndroidJUnitRunner.sApplication
                                .registerActivityLifecycleCallbacks(lifecycleCallbacks);
                    }
                });
        if (remaining.isEmpty()) {
            // There are no activities to destroy.
            return false;
        }

        try {
            doneCallback.waitForFirst();
            // Ensure that all onDestroy() callbacks are dispatched before returning.
            BaseChromiumAndroidJUnitRunner.sInstance.runOnMainSync(() -> {});
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
