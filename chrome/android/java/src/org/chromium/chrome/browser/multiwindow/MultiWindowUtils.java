// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.Display;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.ref.WeakReference;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;

/**
 * Utilities for detecting multi-window/multi-instance support.
 *
 * Thread-safe: This class may be accessed from any thread.
 */
public class MultiWindowUtils implements ActivityStateListener {
    // getInstance() is called early in start-up, so there is not point in lazily initializing it.
    private static final MultiWindowUtils sInstance = AppHooks.get().createMultiWindowUtils();

    private static final boolean SUPPORTS_MULTI_DISPLAY = false;

    // Used to keep track of whether ChromeTabbedActivity2 is running. A tri-state Boolean is
    // used in case both activities die in the background and MultiWindowUtils is recreated.
    private Boolean mTabbedActivity2TaskRunning;
    private WeakReference<ChromeTabbedActivity> mLastResumedTabbedActivity;
    private boolean mIsInMultiWindowModeForTesting;

    /**
     * Returns the singleton instance of MultiWindowUtils.
     */
    public static MultiWindowUtils getInstance() {
        return sInstance;
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in Android N+ multi-window mode.
     */
    public boolean isInMultiWindowMode(Activity activity) {
        if (mIsInMultiWindowModeForTesting) return true;
        if (activity == null) return false;

        return ApiCompatibilityUtils.isInMultiWindowMode(activity);
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether the system currently supports multiple displays.
     */
    public boolean isInMultiDisplayMode(Activity activity) {
        if (!SUPPORTS_MULTI_DISPLAY) return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }
        DisplayManager displayManager =
                (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        Display[] displays = displayManager.getDisplays();
        if (displays == null || displays.length != 2) return false;
        return displays[0].getState() == Display.STATE_ON
                && displays[1].getState() == Display.STATE_ON;
    }

    @VisibleForTesting
    public void setIsInMultiWindowModeForTesting(boolean isInMultiWindowMode) {
        mIsInMultiWindowModeForTesting = isInMultiWindowMode;
    }

    /**
     * Returns whether the given activity currently supports opening tabs in or moving tabs to the
     * other window.
     */
    public boolean isOpenInOtherWindowSupported(Activity activity) {
        if (!isInMultiWindowMode(activity) && !isInMultiDisplayMode(activity)) return false;
        // Supported only in multi-window mode and if activity supports side-by-side instances.
        return getOpenInOtherWindowActivity(activity) != null;
    }

    /**
     * Returns the activity to use when handling "open in other window" or "move to other window".
     * Returns null if the current activity doesn't support opening/moving tabs to another activity.
     */
    public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
        if (current instanceof ChromeTabbedActivity2) {
            // If a second ChromeTabbedActivity is created, MultiWindowUtils needs to listen for
            // activity state changes to facilitate determining which ChromeTabbedActivity should
            // be used for intents.
            ApplicationStatus.registerStateListenerForAllActivities(sInstance);
            return ChromeTabbedActivity.class;
        } else if (current instanceof ChromeTabbedActivity) {
            mTabbedActivity2TaskRunning = true;
            ApplicationStatus.registerStateListenerForAllActivities(sInstance);
            return ChromeTabbedActivity2.class;
        } else {
            return null;
        }
    }

    /**
     * Sets extras on the intent used when handling "open in other window" or
     * "move to other window". Specifically, sets the class, adds the launch adjacent flag, and
     * adds extras so that Chrome behaves correctly when the back button is pressed.
     * @param intent The intent to set details on.
     * @param activity The activity firing the intent.
     * @param targetActivity The class of the activity receiving the intent.
     */
    @TargetApi(Build.VERSION_CODES.N)
    public static void setOpenInOtherWindowIntentExtras(
            Intent intent, Activity activity, Class<? extends Activity> targetActivity) {
        intent.setClass(activity, targetActivity);
        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);

        // Remove LAUNCH_ADJACENT flag if we want to start CTA, but it's already running.
        // If arleady running CTA was started via .Main activity alias, starting it again with
        // LAUNCH_ADJACENT will create another CTA instance with just a single tab. There doesn't
        // seem to be a reliable way to check if an activity was started via an alias, so we're
        // removing the flag if any CTA instance is running. See crbug.com/771516 for details.
        if (targetActivity.equals(ChromeTabbedActivity.class) && isPrimaryTabbedActivityRunning()) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        // Let Chrome know that this intent is from Chrome, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
    }

    /**
     * Generate the activity options used when handling "open in other window" or "move to other
     * window" on a multi-instance capable device.
     *
     * This should be used in combination with
     * {@link #setOpenInOtherWindowIntentExtras(Intent, Activity, Class)}.
     *
     * @param activity The activity firing the intent.
     * @return The ActivityOptions needed to open the content in another display.
     * @see Context#startActivity(Intent, Bundle)
     */
    public static Bundle getOpenInOtherWindowActivityOptions(Activity activity) {
        if (!getInstance().isInMultiDisplayMode(activity)) return null;
        Display defaultDisplay = DisplayAndroidManager.getDefaultDisplayForContext(activity);
        DisplayManager displayManager =
                (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        Display launchDisplay = null;
        for (Display display : displayManager.getDisplays()) {
            if (display.getDisplayId() == defaultDisplay.getDisplayId()) continue;
            launchDisplay = display;
            break;
        }
        if (launchDisplay == null) {
            throw new IllegalStateException(
                    "Attempting to open window in other display, but one is not found");
        }
        return ApiCompatibilityUtils.createLaunchDisplayIdActivityOptions(
                launchDisplay.getDisplayId());
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.RESUMED && activity instanceof ChromeTabbedActivity) {
            mLastResumedTabbedActivity = new WeakReference<>((ChromeTabbedActivity) activity);
        }
    }

    /**
     * Determines the correct ChromeTabbedActivity class to use for an incoming intent.
     * @param intent The incoming intent that is starting ChromeTabbedActivity.
     * @param context The current Context, used to retrieve the ActivityManager system service.
     * @return The ChromeTabbedActivity to use for the incoming intent.
     */
    public Class<? extends ChromeTabbedActivity> getTabbedActivityForIntent(
            @Nullable Intent intent, Context context) {
        // 1. Exit early if the build version doesn't support Android N+ multi-window mode or
        // ChromeTabbedActivity2 isn't running.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M
                || (mTabbedActivity2TaskRunning != null && !mTabbedActivity2TaskRunning)) {
            return ChromeTabbedActivity.class;
        }

        // 2. If the intent has a window id set, use that.
        if (intent != null && IntentUtils.safeHasExtra(intent, IntentHandler.EXTRA_WINDOW_ID)) {
            int windowId = IntentUtils.safeGetIntExtra(intent, IntentHandler.EXTRA_WINDOW_ID, 0);
            if (windowId == 1) return ChromeTabbedActivity.class;
            if (windowId == 2) return ChromeTabbedActivity2.class;
        }

        // 3. If only one ChromeTabbedActivity is currently in Android recents, use it.
        boolean tabbed2TaskRunning = isActivityTaskInRecents(
                ChromeTabbedActivity2.class.getName(), context);

        // Exit early if ChromeTabbedActivity2 isn't running.
        if (!tabbed2TaskRunning) {
            mTabbedActivity2TaskRunning = false;
            return ChromeTabbedActivity.class;
        }

        boolean tabbedTaskRunning = isActivityTaskInRecents(
                ChromeTabbedActivity.class.getName(), context);
        if (!tabbedTaskRunning) {
            return ChromeTabbedActivity2.class;
        }

        // 4. If only one of the ChromeTabbedActivity's is currently visible use it.
        // e.g. ChromeTabbedActivity is docked to the top of the screen and another app is docked
        // to the bottom.

        // Find the activities.
        Activity tabbedActivity = null;
        Activity tabbedActivity2 = null;
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity.getClass().equals(ChromeTabbedActivity.class)) {
                tabbedActivity = activity;
            } else if (activity.getClass().equals(ChromeTabbedActivity2.class)) {
                tabbedActivity2 = activity;
            }
        }

        // Determine if only one is visible.
        boolean tabbedActivityVisible = isActivityVisible(tabbedActivity);
        boolean tabbedActivity2Visible = isActivityVisible(tabbedActivity2);
        if (tabbedActivityVisible ^ tabbedActivity2Visible) {
            if (tabbedActivityVisible) return ChromeTabbedActivity.class;
            return ChromeTabbedActivity2.class;
        }

        // 5. Use the ChromeTabbedActivity that was resumed most recently if it's still running.
        if (mLastResumedTabbedActivity != null) {
            ChromeTabbedActivity lastResumedActivity = mLastResumedTabbedActivity.get();
            if (lastResumedActivity != null) {
                Class<?> lastResumedClassName = lastResumedActivity.getClass();
                if (tabbedTaskRunning
                        && lastResumedClassName.equals(ChromeTabbedActivity.class)) {
                    return ChromeTabbedActivity.class;
                }
                if (tabbed2TaskRunning
                        && lastResumedClassName.equals(ChromeTabbedActivity2.class)) {
                    return ChromeTabbedActivity2.class;
                }
            }
        }

        // 6. Default to regular ChromeTabbedActivity.
        return ChromeTabbedActivity.class;
    }

    /**
     * @param className The class name of the Activity to look for in Android recents
     * @param context The current Context, used to retrieve the ActivityManager system service.
     * @return True if the Activity still has a task in Android recents, regardless of whether
     *         the Activity has been destroyed.
     */
    @TargetApi(Build.VERSION_CODES.M)
    private boolean isActivityTaskInRecents(String className, Context context) {
        ActivityManager activityManager = (ActivityManager)
                context.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        for (AppTask task : appTasks) {
            if (task.getTaskInfo() == null || task.getTaskInfo().baseActivity == null) continue;
            String baseActivity = task.getTaskInfo().baseActivity.getClassName();

            if (TextUtils.equals(baseActivity, ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME)) {
                baseActivity = ChromeTabbedActivity.class.getName();
            }

            if (TextUtils.equals(baseActivity, className)) return true;
        }
        return false;
    }

    /**
     * @param activity The Activity whose visibility to test.
     * @return True iff the given Activity is currently visible.
     */
    public static boolean isActivityVisible(Activity activity) {
        if (activity == null) return false;
        int activityState = ApplicationStatus.getStateForActivity(activity);
        // In Android N multi-window mode, only one activity is resumed at a time. The other
        // activity visible on the screen will be in the paused state. Activities not visible on
        // the screen will be stopped or destroyed.
        return activityState == ActivityState.RESUMED || activityState == ActivityState.PAUSED;
    }

    @VisibleForTesting
    public Boolean getTabbedActivity2TaskRunning() {
        return mTabbedActivity2TaskRunning;
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in pre-N Samsung multi-window mode.
     */
    public boolean isLegacyMultiWindow(Activity activity) {
        if (activity == null) return false;

        try {
            // Check if Samsung's multi-window mode is supported on this device.
            // PackageManager#hasSystemFeature(PackageManager.FEATURE_MULTIWINDOW);
            PackageManager pm = activity.getPackageManager();
            Field multiwindowFeatureField = pm.getClass().getField("FEATURE_MULTIWINDOW");
            if (!pm.hasSystemFeature((String) multiwindowFeatureField.get(null))) return false;

            // Grab the current window mode.
            // int windowMode = Activity#getWindowMode();
            Method getWindowMode = activity.getClass().getMethod("getWindowMode", (Class[]) null);
            int windowMode = (Integer) getWindowMode.invoke(activity, (Object[]) null);

            // Grab the multi-window mode constant.
            // android.view.WindowManagerPolicy#WINDOW_MODE_FREESTYLE
            Class<?> windowManagerPolicyClass = Class.forName("android.view.WindowManagerPolicy");
            Field windowModeFreestyleField =
                    windowManagerPolicyClass.getField("WINDOW_MODE_FREESTYLE");
            int featureMultiWindowFreestyle = (Integer) windowModeFreestyleField.get(null);

            // Compare windowMode with WINDOW_MODE_FREESTYLE to see if that flag is set.
            return (windowMode & featureMultiWindowFreestyle) != 0;
        } catch (NoSuchFieldException e) {
            return false;
        } catch (IllegalAccessException e) {
            return false;
        } catch (IllegalArgumentException e) {
            return false;
        } catch (NoSuchMethodException e) {
            return false;
        } catch (InvocationTargetException e) {
            return false;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    /**
     * @return Whether ChromeTabbedActivity (exact activity, not a subclass of) is currently
     *         running.
     */
    private static boolean isPrimaryTabbedActivityRunning() {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity.getClass().equals(ChromeTabbedActivity.class)) return true;
        }
        return false;
    }

    /**
     * @return Whether or not activity should run in pre-N Samsung multi-instance mode.
     */
    public boolean shouldRunInLegacyMultiInstanceMode(Activity activity, Intent intent) {
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP
                && TextUtils.equals(intent.getAction(), Intent.ACTION_MAIN)
                && isLegacyMultiWindow(activity) && isPrimaryTabbedActivityRunning();
    }

    /**
     * Makes |intent| able to support multi-instance in pre-N Samsung multi-window mode.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void makeLegacyMultiInstanceIntent(Activity activity, Intent intent) {
        if (isLegacyMultiWindow(activity)) {
            if (TextUtils.equals(ChromeTabbedActivity.class.getName(),
                    intent.getComponent().getClassName())) {
                intent.setClassName(activity, MultiInstanceChromeTabbedActivity.class.getName());
            }
            intent.setFlags(intent.getFlags()
                    & ~(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT));
        }
    }
}
