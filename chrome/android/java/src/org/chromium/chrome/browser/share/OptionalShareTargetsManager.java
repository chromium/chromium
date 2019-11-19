// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.ComponentName;
import android.content.pm.PackageManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A manager for optional share activities in the share picker intent.
 */
public class OptionalShareTargetsManager {
    private static final String TAG = "share_manager";

    private static class Holder {
        private static OptionalShareTargetsManager sInstance = new OptionalShareTargetsManager();
    }

    private final TaskRunner mBackgroundTaskRunner;
    private final Set<Activity> mPendingShareActivities;
    private final ActivityStateListener mStateListener;
    private final List<ComponentName> mEnabledComponents;

    private OptionalShareTargetsManager() {
        mBackgroundTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
        mPendingShareActivities = Collections.synchronizedSet(new HashSet<Activity>());
        mStateListener = new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity triggeringActivity, int newState) {
                if (newState == ActivityState.PAUSED) return;
                handleShareFinish(triggeringActivity);
            }
        };
        mEnabledComponents = new ArrayList<>();
    }

    /**
     * @return The singleton OptionalShareTargetsManager.
     */
    public static OptionalShareTargetsManager getInstance() {
        return Holder.sInstance;
    }

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
    public void enableOptionalShareActivities(final Activity triggeringActivity,
            final List<Class<? extends ShareActivity>> enabledClasses, final Runnable callback) {
        ThreadUtils.assertOnUiThread();

        ApplicationStatus.registerStateListenerForAllActivities(mStateListener);
        mPendingShareActivities.add(triggeringActivity);

        mBackgroundTaskRunner.postTask(() -> {
            if (mPendingShareActivities.isEmpty()) return;
            Log.d(TAG, "Enabling %d share components", enabledClasses.size());
            PackageManager pm = triggeringActivity.getPackageManager();
            for (int i = 0; i < enabledClasses.size(); i++) {
                ComponentName newEnabledComponent =
                        new ComponentName(triggeringActivity, enabledClasses.get(i));

                int enabledState = pm.getComponentEnabledSetting(newEnabledComponent);
                if (enabledState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED) continue;

                pm.setComponentEnabledSetting(newEnabledComponent,
                        PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                        PackageManager.DONT_KILL_APP);
                mEnabledComponents.add(newEnabledComponent);
            }
            PostTask.postTask(UiThreadTaskTraits.USER_BLOCKING, callback);
        });
    }

    /**
     * Handles when the triggering activity has finished the sharing operation. If all
     * pending shares have been complete then it will disable all enabled components.
     * @param triggeringActivity The activity that is triggering the share action.
     */
    private void handleShareFinish(final Activity triggeringActivity) {
        ThreadUtils.assertOnUiThread();

        mPendingShareActivities.remove(triggeringActivity);
        if (!mPendingShareActivities.isEmpty()) return;
        ApplicationStatus.unregisterActivityStateListener(mStateListener);

        mBackgroundTaskRunner.postTask(() -> {
            if (!mPendingShareActivities.isEmpty() || mEnabledComponents.isEmpty()) return;
            Log.d(TAG, "Disabling %d enabled share components", mEnabledComponents.size());
            for (int i = 0; i < mEnabledComponents.size(); i++) {
                triggeringActivity.getPackageManager().setComponentEnabledSetting(
                        mEnabledComponents.get(i), PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                        PackageManager.DONT_KILL_APP);
            }
            mEnabledComponents.clear();
        });
    }
}
