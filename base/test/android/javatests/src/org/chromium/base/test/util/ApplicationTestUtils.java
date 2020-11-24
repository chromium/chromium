// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.app.Activity;
import android.support.test.runner.lifecycle.ActivityLifecycleCallback;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitor;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Methods used for testing Application-level behavior.
 */
public class ApplicationTestUtils {
    private static final ActivityLifecycleMonitor sMonitor =
            ActivityLifecycleMonitorRegistry.getInstance();

    /** Waits until the given activity transitions to the given state. */
    public static void waitForActivityState(Activity activity, Stage stage) {
        waitForActivityState(null, activity, stage);
    }

    /** Waits until the given activity transitions to the given state. */
    public static void waitForActivityState(String failureReason, Activity activity, Stage stage) {
        CriteriaHelper.pollUiThread(
                ()
                        -> { return sMonitor.getLifecycleStageOf(activity) == stage; },
                failureReason, ScalableTimeout.scaleTimeout(10000),
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Finishes the given activity and waits for its onDestroy() to be called. */
    public static void finishActivity(final Activity activity) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (sMonitor.getLifecycleStageOf(activity) != Stage.DESTROYED) {
                activity.finish();
            }
        });
        waitForActivityState(
                "Failed to finish the Activity. Did you start a second Activity and not finish it?",
                activity, Stage.DESTROYED);
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
     * @param state The Activity {@link Stage} to wait for an activity of the right class type to
     * reach.
     * @param trigger The Runnable that will trigger the state change to wait for.
     */
    public static <T extends Activity> T waitForActivityWithClass(
            Class<? extends Activity> activityClass, Stage stage, Runnable trigger) {
        final CallbackHelper activityCallback = new CallbackHelper();
        final AtomicReference<T> activityRef = new AtomicReference<>();
        ActivityLifecycleCallback stateListener = (Activity newActivity, Stage newStage) -> {
            if (newStage == stage) {
                if (!activityClass.isAssignableFrom(newActivity.getClass())) return;

                activityRef.set((T) newActivity);
                ThreadUtils.postOnUiThread(() -> activityCallback.notifyCalled());
            }
        };
        sMonitor.addLifecycleCallback(stateListener);

        try {
            ThreadUtils.runOnUiThreadBlocking(() -> trigger.run());
            activityCallback.waitForCallback("No Activity reached target state.", 0);
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
