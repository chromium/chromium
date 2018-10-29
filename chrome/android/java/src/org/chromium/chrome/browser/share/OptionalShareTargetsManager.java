// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.os.StrictMode;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/**
 * A manager for optional share activities in the share picker intent.
 */
public class OptionalShareTargetsManager {
    private static final String TAG = "share_manager";

    private static Set<Activity> sPendingShareActivities =
            Collections.synchronizedSet(new HashSet<Activity>());
    private static ActivityStateListener sStateListener;
    private static AsyncTask<Void> sStateChangeTask;
    private static List<ComponentName> sEnabledComponents;

    /**
     * Enables sharing options.
     * @param triggeringActivity The activity that will be triggering the share action. The
     *                 activity's state will be tracked to disable the options when
     *                 the share operation has been completed.
     * @param enabledClasses classes to be enabled.
     * @param callback The callback to be triggered after the options have been enabled.  This
     *                 may or may not be synchronous depending on whether this will require
     *                 interacting with the Android framework.
     */
    public static void enableOptionalShareActivities(final Activity triggeringActivity,
            final List<Class<? extends ShareActivity>> enabledClasses, final Runnable callback) {
        ThreadUtils.assertOnUiThread();

        if (sStateListener == null) {
            sStateListener = new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity triggeringActivity, int newState) {
                    if (newState == ActivityState.PAUSED) return;
                    handleShareFinish(triggeringActivity);
                }
            };
        }
        ApplicationStatus.registerStateListenerForAllActivities(sStateListener);
        boolean wasEmpty = sPendingShareActivities.isEmpty();
        sPendingShareActivities.add(triggeringActivity);

        waitForPendingStateChangeTask();
        if (wasEmpty) {
            // Note: possible race condition if two calls to this method happen simultaneously.
            sStateChangeTask = new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    if (sPendingShareActivities.isEmpty()) return null;
                    sEnabledComponents = new ArrayList<>(enabledClasses.size());
                    for (int i = 0; i < enabledClasses.size(); i++) {
                        ComponentName newEnabledComponent =
                                new ComponentName(triggeringActivity, enabledClasses.get(i));
                        triggeringActivity.getPackageManager().setComponentEnabledSetting(
                                newEnabledComponent, PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                                PackageManager.DONT_KILL_APP);
                        sEnabledComponents.add(newEnabledComponent);
                    }
                    return null;
                }

                @Override
                protected void onPostExecute(Void result) {
                    if (sStateChangeTask == this) {
                        sStateChangeTask = null;
                    } else {
                        waitForPendingStateChangeTask();
                    }
                    callback.run();
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            callback.run();
        }
    }

    /**
     * Handles when the triggering activity has finished the sharing operation. If all
     * pending shares have been complete then it will disable all enabled components.
     * @param triggeringActivity The activity that is triggering the share action.
     */
    public static void handleShareFinish(final Activity triggeringActivity) {
        ThreadUtils.assertOnUiThread();

        sPendingShareActivities.remove(triggeringActivity);
        if (!sPendingShareActivities.isEmpty()) return;
        ApplicationStatus.unregisterActivityStateListener(sStateListener);

        waitForPendingStateChangeTask();
        sStateChangeTask = new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                if (!sPendingShareActivities.isEmpty() || sEnabledComponents == null) return null;
                for (int i = 0; i < sEnabledComponents.size(); i++) {
                    triggeringActivity.getPackageManager().setComponentEnabledSetting(
                            sEnabledComponents.get(i),
                            PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                            PackageManager.DONT_KILL_APP);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                if (sStateChangeTask == this) sStateChangeTask = null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Waits for any pending state change operations to be completed.
     *
     * This will avoid timing issues described here: crbug.com/649453.
     */
    private static void waitForPendingStateChangeTask() {
        ThreadUtils.assertOnUiThread();

        if (sStateChangeTask == null) return;
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            sStateChangeTask.get();
            sStateChangeTask = null;
        } catch (InterruptedException | ExecutionException e) {
            Log.e(TAG, "State change task did not complete as expected");
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }
}
