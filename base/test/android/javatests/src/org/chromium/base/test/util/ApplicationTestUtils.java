// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
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
        CriteriaHelper.pollUiThread(() -> {
            return sMonitor.getLifecycleStageOf(activity) == stage;
        }, failureReason, 10000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Finishes the given activity and waits for its onDestroy() to be called. */
    public static void finishActivity(final Activity activity) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (sMonitor.getLifecycleStageOf(activity) != Stage.DESTROYED) {
                activity.finish();
            }
        });
        final String error = "Failed to finish the Activity. Did you start a second Activity and "
                + "not finish it?";
        try {
            waitForActivityState(error, activity, Stage.DESTROYED);
        } catch (Throwable e) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) throw e;

            // On L, there's a framework bug where Activities sometimes just don't get finished
            // unless you start another Activity.
            Intent intent = new Intent(Settings.ACTION_SETTINGS);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(intent);
            try {
                waitForActivityState(error, activity, Stage.DESTROYED);
            } finally {
                // We can't finish com.android.settings, so return to launcher instead.
                fireHomescreenIntent(activity);
            }
        }
    }

    private static void fireHomescreenIntent(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
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
     *         reach.
     * @param uiThreadTrigger The Runnable that will trigger the state change to wait for. The
     *         Runnable will be run on the UI thread
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
     * @param state The Activity {@link Stage} to wait for an activity of the right class type to
     *         reach.
     * @param uiThreadTrigger The Runnable that will trigger the state change to wait for, which
     *         will be run on the UI thread.
     * @param backgroundThreadTrigger The Runnable that will trigger the state change to wait for,
     *         which will be run on the UI thread.
     */
    public static <T extends Activity> T waitForActivityWithClass(
            Class<? extends Activity> activityClass, Stage stage, Runnable uiThreadTrigger,
            Runnable backgroundThreadTrigger) {
        ThreadUtils.assertOnBackgroundThread();
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
            if (uiThreadTrigger != null) {
                ThreadUtils.runOnUiThreadBlocking(() -> uiThreadTrigger.run());
            }
            if (backgroundThreadTrigger != null) backgroundThreadTrigger.run();
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
