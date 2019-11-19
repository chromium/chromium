// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;

import org.junit.rules.ExternalResource;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.TimeoutException;

/**
 * This is to ensure all calls to onDestroy() are performed before starting the next test.
 * We could probably remove this when crbug.com/932130 is fixed.
 */
public class DestroyActivitiesRule extends ExternalResource {
    private static final String TAG = "DestroyActivities";
    private final Set<Activity> mBlacklistedActivities =
            Collections.newSetFromMap(new WeakHashMap<>());

    private boolean allActivitiesDestroyedOrBlacklisted() {
        if (ApplicationStatus.isEveryActivityDestroyed()) {
            return true;
        }
        for (Activity a : ApplicationStatus.getRunningActivities()) {
            if (!mBlacklistedActivities.contains(a)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void after() {
        if (!ApplicationStatus.isInitialized()) {
            return;
        }
        CallbackHelper allDestroyedCalledback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activityStateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.DESTROYED:
                                if (allActivitiesDestroyedOrBlacklisted()) {
                                    allDestroyedCalledback.notifyCalled();
                                    ApplicationStatus.unregisterActivityStateListener(this);
                                }
                                break;
                            case ActivityState.CREATED:
                                if (!activity.isFinishing()) {
                                    // This is required to ensure we finish any activities created
                                    // after doing the bulk finish operation below.
                                    activity.finish();
                                }
                                break;
                        }
                    }
                };

        ThreadUtils.runOnUiThread(() -> {
            if (allActivitiesDestroyedOrBlacklisted()) {
                allDestroyedCalledback.notifyCalled();
            } else {
                ApplicationStatus.registerStateListenerForAllActivities(activityStateListener);
            }
            for (Activity a : ApplicationStatus.getRunningActivities()) {
                if (!a.isFinishing() && !mBlacklistedActivities.contains(a)) {
                    a.finish();
                }
            }
        });
        try {
            allDestroyedCalledback.waitForFirst();
        } catch (TimeoutException e) {
            // There appears to be a framework bug on K and L where onStop and onDestroy are not
            // called for a handful of tests. We ignore these exceptions.
            Log.w(TAG, "Activity failed to be destroyed after a test");

            ThreadUtils.runOnUiThreadBlocking(() -> {
                mBlacklistedActivities.addAll(ApplicationStatus.getRunningActivities());

                // Make sure subsequent tests don't have these notifications firing.
                ApplicationStatus.unregisterActivityStateListener(activityStateListener);
            });
        }
    }
}
