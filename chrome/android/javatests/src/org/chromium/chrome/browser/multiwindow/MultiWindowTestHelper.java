// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utilities helper class for multi-window tests.
 */
public class MultiWindowTestHelper {
    /**
     * Creates a new {@link ChromeTabbedActivity2} with no {@link LoadUrlParams}.
     * @param activity A current running activity that will handle the intent to start another
     *                 activity.
     * @return The new {@link ChromeTabbedActivity2}.
     */
    public static ChromeTabbedActivity2 createSecondChromeTabbedActivity(Activity activity) {
        return createSecondChromeTabbedActivity(activity, null);
    }

    /**
     * Creates a new {@link ChromeTabbedActivity2}.
     * @param activity A current running activity that will handle the intent to start another
     *                 activity.
     * @param params {@link LoadUrlParams} used to create the launch intent. May be null if no
     *               params should be used when creating the activity.
     * @return The new {@link ChromeTabbedActivity2}.
     */
    public static ChromeTabbedActivity2 createSecondChromeTabbedActivity(
            Activity activity, @Nullable LoadUrlParams params) {
        // TODO(twellington): after there is test support for putting an activity into multi-window
        // mode, this should be changed to use the menu item for opening a new window.

        // Number of expected activities after the second ChromeTabbedActivity is created.
        int numExpectedActivities = ApplicationStatus.getRunningActivities().size() + 1;

        // Get the class name to use for the second ChromeTabbedActivity. This step is important
        // for initializing things in MultiWindowUtils.java.
        Class<? extends Activity> secondActivityClass =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(activity);
        Assert.assertEquals(
                "ChromeTabbedActivity2 should be used as the 'open in other window' activity.",
                ChromeTabbedActivity2.class, secondActivityClass);

        // Create an intent and start the second ChromeTabbedActivity.
        Intent intent;
        if (params != null) {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.getUrl()));
        } else {
            intent = new Intent();
        }

        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, activity, secondActivityClass);
        MultiInstanceManager.onMultiInstanceModeStarted();
        activity.startActivity(intent);

        // Wait for ChromeTabbedActivity2 to be created.
        CriteriaHelper.pollUiThread(Criteria.equals(numExpectedActivities, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ApplicationStatus.getRunningActivities().size();
            }
        }));

        return waitForSecondChromeTabbedActivity();
    }

    /**
     * Waits for an instance of ChromeTabbedActivity2, and then waits until it's resumed.
     */
    public static ChromeTabbedActivity2 waitForSecondChromeTabbedActivity() {
        AtomicReference<ChromeTabbedActivity2> returnActivity = new AtomicReference<>();
        CriteriaHelper.pollUiThread(new Criteria(
                "Couldn't find instance of ChromeTabbedActivity2") {
            @Override
            public boolean isSatisfied() {
                for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                    if (runningActivity.getClass().equals(ChromeTabbedActivity2.class)) {
                        returnActivity.set((ChromeTabbedActivity2) runningActivity);
                        return true;
                    }
                }
                return false;
            }
        });
        waitUntilActivityResumed(returnActivity.get());
        return returnActivity.get();
    }

    private static void waitUntilActivityResumed(final Activity activity) {
        CriteriaHelper.pollUiThread(Criteria.equals(ActivityState.RESUMED, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ApplicationStatus.getStateForActivity(activity);
            }
        }));
    }

    /**
     * Moves the given activity to the foreground so it can receive user input.
     */
    @TargetApi(Build.VERSION_CODES.N)
    public static void moveActivityToFront(final Activity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Context context = ContextUtils.getApplicationContext();
            ActivityManager activityManager =
                    (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                if (activity.getTaskId() == task.getTaskInfo().id) {
                    task.moveToFront();
                    break;
                }
            }
        });
        waitUntilActivityResumed(activity);
    }

    /**
     * Waits until 'activity' has 'expectedTotalTabCount' total tabs, and its active tab has
     * 'expectedActiveTabId' ID. 'expectedActiveTabId' is optional and can be Tab.INVALID_TAB_ID.
     * 'tag' is an arbitrary string that is prepended to failure reasons.
     */
    public static void waitForTabs(final String tag, final ChromeTabbedActivity activity,
            final int expectedTotalTabCount, final int expectedActiveTabId) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int actualTotalTabCount = activity.getTabModelSelector().getTotalTabCount();
                if (expectedTotalTabCount != actualTotalTabCount) {
                    updateFailureReason(String.format((Locale) null,
                            "%s: total tab count is wrong: %d (expected %d)", tag,
                            actualTotalTabCount, expectedTotalTabCount));
                    return false;
                }
                return true;
            }
        });

        if (expectedActiveTabId == Tab.INVALID_TAB_ID) return;

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int actualActiveTabId = activity.getActivityTab().getId();
                if (expectedActiveTabId != actualActiveTabId) {
                    updateFailureReason(String.format((Locale) null,
                            "%s: active tab ID is wrong: %d (expected %d)", tag, actualActiveTabId,
                            expectedActiveTabId));
                    return false;
                }
                return true;
            }
        });
    }
}
