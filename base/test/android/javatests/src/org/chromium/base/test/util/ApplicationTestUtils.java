// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.app.Activity;

import androidx.test.runner.lifecycle.ActivityLifecycleCallback;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitor;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Methods used for testing Application-level behavior. */
public class ApplicationTestUtils {
    private static final String TAG = "ApplicationTestUtils";
    private static final ActivityLifecycleMonitor sMonitor =
            ActivityLifecycleMonitorRegistry.getInstance();

    private static final long ACTIVITY_TIMEOUT = 10000;

    /** Waits until the given activity transitions to the given state. */
    public static void waitForActivityState(Activity activity, Stage stage) {
        waitForActivityState(
                "Activity "
                        + activity.getLocalClassName()
                        + " did not reach stage: "
                        + stage
                        + ". Is the device screen turned on?",
                activity,
                stage);
    }

    /** Waits until the given activity transitions to the given state. */
    public static void waitForActivityState(String failureReason, Activity activity, Stage stage) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return sMonitor.getLifecycleStageOf(activity) == stage;
                },
                failureReason,
                ACTIVITY_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // De-flake by flushing the tasks that are already queued on the Looper's Handler.
        // TODO(crbug.com/40260566): Remove this and properly fix flaky tests.
        TestThreadUtils.flushNonDelayedLooperTasks();
    }

    /** Finishes the given activity and waits for its onDestroy() to be called. */
    public static void finishActivity(final Activity activity) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (sMonitor.getLifecycleStageOf(activity) != Stage.DESTROYED) {
                        Log.i(TAG, "Finishing %s", activity);
                        activity.finish();
                    } else {
                        Log.i(TAG, "Not finishing - already destroyed: %s", activity);
                    }
                });
        final String error =
                "Failed to finish the Activity. Did you start a second Activity and "
                        + "not finish it?";
        waitForActivityState(error, activity, Stage.DESTROYED);
    }

    /**
     * Recreates the provided Activity, returning the newly created Activity once it's finished
     * starting up.
     * @param activity The Activity to recreate.
     * @return The newly created Activity.
     */
    public static <T extends Activity> T recreateActivity(T activity) {
        return waitForActivityWithClass(
                activity.getClass(), Stage.RESUMED, () -> activity.recreate());
    }

    /**
     * Waits for an activity of the specified class to reach the specified Activity {@link Stage},
     * triggered by running the provided trigger.
     *
     * @param activityClass The class type to wait for.
     * @param stage The Activity {@link Stage} to wait for an activity of the right class type to
     *     reach.
     * @param uiThreadTrigger The Runnable that will trigger the state change to wait for. The
     *     Runnable will be run on the UI thread
     */
    public static <T extends Activity> T waitForActivityWithClass(
            Class<? extends Activity> activityClass, Stage stage, Runnable uiThreadTrigger) {
        return waitForActivityWithClass(activityClass, stage, uiThreadTrigger, null);
    }

    /**
     * Waits for an activity of the specified class to reach the specified Activity {@link Stage},
     * triggered by running the provided trigger.
     *
     * @param activityClass The class type to wait for.
     * @param stage The Activity {@link Stage} to wait for an activity of the right class type to
     *     reach.
     * @param uiThreadTrigger The Runnable that will trigger the state change to wait for, which
     *     will be run on the UI thread.
     * @param backgroundThreadTrigger The Runnable that will trigger the state change to wait for,
     *     which will be run on the UI thread.
     */
    public static <T extends Activity> T waitForActivityWithClass(
            Class<? extends Activity> activityClass,
            Stage stage,
            Runnable uiThreadTrigger,
            Runnable backgroundThreadTrigger) {
        ThreadUtils.assertOnBackgroundThread();
        final CallbackHelper activityCallback = new CallbackHelper();
        final AtomicReference<T> activityRef = new AtomicReference<>();
        ActivityLifecycleCallback stateListener =
                (Activity newActivity, Stage newStage) -> {
                    if (newStage == stage) {
                        if (!activityClass.isAssignableFrom(newActivity.getClass())) return;

                        activityRef.set((T) newActivity);
                        ThreadUtils.postOnUiThread(() -> activityCallback.notifyCalled());
                    }
                };
        sMonitor.addLifecycleCallback(stateListener);

        try {
            if (uiThreadTrigger != null) {
                ThreadUtils.runOnUiThreadBlocking(() -> uiThreadTrigger.run());
            }
            if (backgroundThreadTrigger != null) backgroundThreadTrigger.run();
            activityCallback.waitForOnly(
                    "No Activity reached target state.", ACTIVITY_TIMEOUT, TimeUnit.MILLISECONDS);
            T createdActivity = activityRef.get();
            Assert.assertNotNull("Activity reference is null.", createdActivity);
            return createdActivity;
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        } finally {
            sMonitor.removeLifecycleCallback(stateListener);
        }
    }
}
