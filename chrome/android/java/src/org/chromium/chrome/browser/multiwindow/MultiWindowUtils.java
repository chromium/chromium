// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.SparseBooleanArray;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Locale;

/**
 * Utilities for detecting multi-window/multi-instance support.
 *
 * Thread-safe: This class may be accessed from any thread.
 */
public class MultiWindowUtils implements ActivityStateListener {
    public static final int INVALID_INSTANCE_ID = TabWindowManager.INVALID_WINDOW_INDEX;
    public static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    private static MultiWindowUtils sInstance = new MultiWindowUtils();

    private final boolean mMultiInstanceApi31Enabled;

    // Used to keep track of whether ChromeTabbedActivity2 is running. A tri-state Boolean is
    // used in case both activities die in the background and MultiWindowUtils is recreated.
    private Boolean mTabbedActivity2TaskRunning;
    private WeakReference<ChromeTabbedActivity> mLastResumedTabbedActivity;
    private boolean mIsInMultiWindowModeForTesting;

    // Note: these values must match the AndroidMultiWindowActivityType enum in enums.xml.
    @IntDef({MultiWindowActivityType.ENTER, MultiWindowActivityType.EXIT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MultiWindowActivityType {
        int ENTER = 0;
        int EXIT = 1;
    }

    // Note: these values must match the AndroidMultiWindowState enum in enums.xml.
    @IntDef({MultiWindowState.SINGLE_WINDOW, MultiWindowState.MULTI_WINDOW})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MultiWindowState {
        int SINGLE_WINDOW = 0;
        int MULTI_WINDOW = 1;
    }

    protected MultiWindowUtils() {
        mMultiInstanceApi31Enabled = isMultiInstanceApi31Enabled();
    }

    /**
     * @return Whether the feature flag is on to enable instance switcher UI/menu.
     */
    public static boolean instanceSwitcherEnabled() {
        // Instance switcher is supported on S, and on some R platforms where the new
        // launch mode is backported.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return false;
        return ChromeFeatureList.sInstanceSwitcher.isEnabled();
    }

    /**
     * @return Whether the new launch mode 'singleInstancePerTask' is configured to allow
     *         multiple instantiation of Chrome instance.
     */
    public static boolean isMultiInstanceApi31Enabled() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return false;
        Context context = ContextUtils.getApplicationContext();
        String packageName = context.getPackageName();
        String className = ChromeTabbedActivity.class.getCanonicalName();
        ComponentName comp = new ComponentName(packageName, className);
        try {
            int launchMode = context.getPackageManager().getActivityInfo(comp, 0).launchMode;
            return launchMode == ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    public static int getMaxInstances() {
        return isMultiInstanceApi31Enabled() ? TabWindowManager.MAX_SELECTORS_S
                                             : TabWindowManager.MAX_SELECTORS_LEGACY;
    }

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
     * @return Whether the system currently supports multiple displays, requiring Android Q+.
     */
    public boolean isInMultiDisplayMode(Activity activity) {
        // TODO(crbug.com/824954): Consider supporting more displays.
        return ApiCompatibilityUtils.getTargetableDisplayIds(activity).size() == 2;
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

        return getOpenInOtherWindowActivity(activity) != null;
    }

    /**
     * See if Chrome can get itself into multi-window mode.
     * @param activity The {@link Activity} to check.
     * @return {@code True} if Chrome can get itself into multi-window mode.
     */
    public boolean canEnterMultiWindowMode(Activity activity) {
        return aospMultiWindowModeSupported() || customMultiWindowModeSupported();
    }

    @VisibleForTesting
    boolean aospMultiWindowModeSupported() {
        // Auto screen splitting works from sc-v2.
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.S
                || Build.VERSION.CODENAME.equals("Sv2");
    }

    @VisibleForTesting
    boolean customMultiWindowModeSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && Build.MANUFACTURER.toUpperCase(Locale.ENGLISH).equals("SAMSUNG");
    }

    /**
     * Returns the activity to use when handling "open in other window" or "move to other window".
     * Returns null if the current activity doesn't support opening/moving tabs to another activity.
     */
    public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
        // Use always ChromeTabbedActivity when multi-instance support in S+ is enabled.
        if (mMultiInstanceApi31Enabled) return ChromeTabbedActivity.class;
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
    public static void setOpenInOtherWindowIntentExtras(
            Intent intent, Activity activity, Class<? extends Activity> targetActivity) {
        intent.setClass(activity, targetActivity);
        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);

        // Remove LAUNCH_ADJACENT flag if we want to start CTA, but it's already running.
        // If arleady running CTA was started via .Main activity alias, starting it again with
        // LAUNCH_ADJACENT will create another CTA instance with just a single tab. There doesn't
        // seem to be a reliable way to check if an activity was started via an alias, so we're
        // removing the flag if any CTA instance is running. See crbug.com/771516 for details.
        if (!isMultiInstanceApi31Enabled() && targetActivity.equals(ChromeTabbedActivity.class)
                && isPrimaryTabbedActivityRunning()) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        // Let Chrome know that this intent is from Chrome, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
    }

    /**
     * Creates and returns an {@link Intent} that instantiates a new Chrome instance.
     * @param context The application context of the activity firing the intent.
     * @param instanceId ID of the new Chrome instance to be created.
     * @param preferNew {@code true} if the new instance should be instanted as a fresh
     *        new one not loading any tabs from a persistent disk file.
     * @param openAdjacently {@code true} if the new instance shall be created in
     *        the adjacent window of split-screen mode.
     * @return The created intent.
     */
    public static Intent createNewWindowIntent(
            Context context, int instanceId, boolean preferNew, boolean openAdjacently) {
        assert isMultiInstanceApi31Enabled();
        Intent intent = new Intent(context, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        if (instanceId != INVALID_INSTANCE_ID) {
            intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, instanceId);
        }
        if (preferNew) intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        if (openAdjacently) intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
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
        int id = getDisplayIdForTargetableSecondaryDisplay(activity);
        if (id == Display.INVALID_DISPLAY) {
            throw new IllegalStateException(
                    "Attempting to open window in other display, but one is not found");
        }
        return ApiCompatibilityUtils.createLaunchDisplayIdActivityOptions(id);
    }

    /**
     * Find a display which can launch a chrome instance.
     *
     * @param activity The activity looking for a secondary display.
     * @return The targetable secondary display. {@code Display.INVALID_DISPLAY} if not found.
     */
    public static int getDisplayIdForTargetableSecondaryDisplay(Activity activity) {
        List<Integer> displays = ApiCompatibilityUtils.getTargetableDisplayIds(activity);
        Display defaultDisplay = DisplayAndroidManager.getDefaultDisplayForContext(activity);
        if (displays.size() != 0) {
            for (int id : displays) {
                if (id != defaultDisplay.getDisplayId()) {
                    return id;
                }
            }
        }
        return Display.INVALID_DISPLAY;
    }

    /**
     * @return The number of Chrome instances that can switch to or launch.
     */
    public static int getInstanceCount() {
        int count = 0;
        for (int i = 0; i < getMaxInstances(); ++i) {
            if (MultiInstanceManagerApi31.instanceEntryExists(i) && isRestorableInstance(i)) {
                count++;
            }
        }
        return count;
    }

    /**
     * @return Whether the app menu 'Manage windows' should be shown.
     */
    public static boolean shouldShowManageWindowsMenu() {
        return getInstanceCount() > 1;
    }

    static boolean isRestorableInstance(int index) {
        return MultiInstanceManagerApi31.readTabCount(index) != 0
                || MultiInstanceManagerApi31.getTaskFromMap(index) != INVALID_TASK_ID;
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.RESUMED && activity instanceof ChromeTabbedActivity) {
            mLastResumedTabbedActivity = new WeakReference<>((ChromeTabbedActivity) activity);
        }
    }

    /**
     * Determines the name of an activity from its {@link AppTask}.
     * @param task The AppTask to get the name of.
     */
    public static String getActivityNameFromTask(AppTask task) {
        ActivityManager.RecentTaskInfo taskInfo = AndroidTaskUtils.getTaskInfoFromTask(task);
        if (taskInfo == null || taskInfo.baseActivity == null) return "";

        String baseActivity = taskInfo.baseActivity.getClassName();
        // Contrary to the documentation taskInfo.baseActivity for the .LauncherMain
        // activity alias is the alias itself, and not the implementation. Filed b/66729258;
        // for now translate the alias manually.
        if (TextUtils.equals(baseActivity, ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME)) {
            baseActivity = ChromeTabbedActivity.class.getName();
        }
        return baseActivity;
    }

    /**
     * @param current Current activity trying to find its adjacent one.
     * @return ChromeTabbedActivity instance of the task running adjacently to the current one.
     *         {@code null} if there is no such task.
     */
    public static Activity getAdjacentWindowActivity(Activity current) {
        List<Activity> runningActivities = ApplicationStatus.getRunningActivities();
        int currentTaskId = current.getTaskId();
        for (Activity activity : runningActivities) {
            int taskId = activity.getTaskId();
            if (taskId != currentTaskId && isActivityVisible(activity)) {
                // Found a visible task. Return its base ChromeTabbedActivity instance.
                for (Activity a : runningActivities) {
                    if (a instanceof ChromeTabbedActivity && a.getTaskId() == taskId) return a;
                }
                assert false : "Should have found the ChromeTabbedActivity of the visible task";
                break;
            }
        }
        return null;
    }

    /**
     * Determines if multiple instances of Chrome are running.
     * @param context The current Context, used to retrieve the ActivityManager system service.
     * @return True if multiple instances of Chrome are running.
     */
    public boolean areMultipleChromeInstancesRunning(Context context) {
        // Check if both tasks are running.
        boolean tabbedTaskRunning = false;
        boolean tabbed2TaskRunning = false;
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity.getClass().equals(ChromeTabbedActivity.class)) {
                tabbedTaskRunning = true;
            } else if (activity.getClass().equals(ChromeTabbedActivity2.class)) {
                tabbed2TaskRunning = true;
            }
        }
        if (tabbedTaskRunning && tabbed2TaskRunning) return true;

        // If a task isn't running check if it is in recents since another instance could be
        // recovered from there.
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        for (AppTask task : appTasks) {
            String baseActivity = getActivityNameFromTask(task);

            if (TextUtils.equals(baseActivity, ChromeTabbedActivity.class.getName())) {
                tabbedTaskRunning = true;
            } else if (TextUtils.equals(baseActivity, ChromeTabbedActivity2.class.getName())) {
                tabbed2TaskRunning = true;
            }
        }
        return tabbedTaskRunning && tabbed2TaskRunning;
    }

    /**
     * Determines the correct ChromeTabbedActivity class to use for an incoming intent.
     * @param intent The incoming intent that is starting ChromeTabbedActivity.
     * @param context The current Context, used to retrieve the ActivityManager system service.
     * @return The ChromeTabbedActivity to use for the incoming intent.
     */
    public Class<? extends ChromeTabbedActivity> getTabbedActivityForIntent(
            @Nullable Intent intent, Context context) {
        // 0. Use always ChromeTabbedActivity when multi-instance support in S+ is enabled.
        if (mMultiInstanceApi31Enabled) return ChromeTabbedActivity.class;

        // 1. Exit early if ChromeTabbedActivity2 isn't running.
        if (mTabbedActivity2TaskRunning != null && !mTabbedActivity2TaskRunning) {
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
    private boolean isActivityTaskInRecents(String className, Context context) {
        ActivityManager activityManager = (ActivityManager)
                context.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        for (AppTask task : appTasks) {
            String baseActivity = getActivityNameFromTask(task);

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

    /**
     * @returns A map taskID : boolean containing the visible tasks.
     */
    public static SparseBooleanArray getVisibleTasks() {
        SparseBooleanArray visibleTasks = new SparseBooleanArray();
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        for (Activity activity : activities) {
            if (isActivityVisible(activity)) visibleTasks.put(activity.getTaskId(), true);
        }
        return visibleTasks;
    }

    /**
     * @param currentActivity Current {@link Activity} in the foreground.
     * @return Whether there is an activity, other than the current one, that is running
     *         in the foreground.
     */
    public boolean isChromeRunningInAdjacentWindow(Activity currentActivity) {
        return getAdjacentWindowActivity(currentActivity) != null;
    }

    /**
     * @return The number of visible tasks running ChromeTabbedActivity.
     */
    public static int getVisibleTabbedTaskCount() {
        SparseBooleanArray ctaTasks = getAllChromeTabbedTasks();
        SparseBooleanArray visibleTasks = getVisibleTasks();
        int visibleCtaCount = 0;
        for (int i = 0; i < visibleTasks.size(); ++i) {
            int task = visibleTasks.keyAt(i);
            if (ctaTasks.get(task) && visibleTasks.valueAt(i)) visibleCtaCount++;
        }
        return visibleCtaCount;
    }

    private static SparseBooleanArray getAllChromeTabbedTasks() {
        SparseBooleanArray ctaTasks = new SparseBooleanArray();
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        for (Activity activity : activities) {
            if (activity instanceof ChromeTabbedActivity) ctaTasks.put(activity.getTaskId(), true);
        }
        return ctaTasks;
    }

    static String lastAccessedTimeKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                String.valueOf(index));
    }

    /**
     * Read the time when an instance was last accessed.
     * @param index Instance ID
     * @return The time when the instance was last accessed.
     */
    static long readLastAccessedTime(int index) {
        return SharedPreferencesManager.getInstance().readLong(lastAccessedTimeKey(index));
    }

    /**
     * Write the time this instance is accessed.
     * @param index Instance ID
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void writeLastAccessedTime(int index) {
        SharedPreferencesManager.getInstance().writeLong(
                lastAccessedTimeKey(index), System.currentTimeMillis());
    }

    @VisibleForTesting
    public Boolean getTabbedActivity2TaskRunning() {
        return mTabbedActivity2TaskRunning;
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
     * Records user actions and ukms associated with entering and exiting Android N multi-window
     * mode.
     * For second activity, records separate user actions for entering/exiting multi-window mode to
     * avoid recording the same action twice when two instances are running, but still records same
     * UKM since two instances have two different tabs.
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     * @param isDeferredStartup True if the activity is deferred startup.
     * @param isFirstActivity True if the activity is the first activity in multi-window mode.
     * @param tab The current activity {@link Tab}.
     */
    public void recordMultiWindowModeChanged(boolean isInMultiWindowMode, boolean isDeferredStartup,
            boolean isFirstActivity, @Nullable Tab tab) {
        if (isFirstActivity) {
            if (isInMultiWindowMode) {
                if (mMultiInstanceApi31Enabled) {
                    SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
                    long startTime = prefs.readLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME);
                    if (startTime == 0) {
                        RecordUserAction.record("Android.MultiWindowMode.Enter2");
                        long current = System.currentTimeMillis();
                        prefs.writeLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME, current);
                    }
                } else {
                    RecordUserAction.record("Android.MultiWindowMode.Enter2");
                }
            } else {
                if (mMultiInstanceApi31Enabled) {
                    SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
                    long startTime = prefs.readLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME);
                    if (startTime > 0) {
                        long current = System.currentTimeMillis();
                        RecordUserAction.record("Android.MultiWindowMode.Exit2");
                        RecordHistogram.recordLongTimesHistogram(
                                "Android.MultiWindowMode.TotalDuration", current - startTime);
                        prefs.writeLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME, 0);
                    }
                } else {
                    RecordUserAction.record("Android.MultiWindowMode.Exit2");
                }
            }
        } else {
            if (isDeferredStartup) {
                RecordUserAction.record("Android.MultiWindowMode.MultiInstance.Enter");
            } else if (isInMultiWindowMode) {
                RecordUserAction.record("Android.MultiWindowMode.Enter-SecondInstance");
            } else {
                RecordUserAction.record("Android.MultiWindowMode.Exit-SecondInstance");
            }
        }

        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge().recordEventWithIntegerMetric(tab.getWebContents(),
                "Android.MultiWindowChangeActivity", "ActivityType",
                isInMultiWindowMode ? MultiWindowActivityType.ENTER : MultiWindowActivityType.EXIT);
    }

    /**
     * Records the ukms about if the activity is in multi-window mode when the activity is shown.
     * @param activity The current Context, used to retrieve the ActivityManager system service.
     * @param tab The current activity {@link Tab}.
     */
    public void recordMultiWindowStateUkm(Activity activity, Tab tab) {
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge().recordEventWithIntegerMetric(tab.getWebContents(),
                "Android.MultiWindowState", "WindowState",
                isInMultiWindowMode(activity) ? MultiWindowState.MULTI_WINDOW
                                              : MultiWindowState.SINGLE_WINDOW);
    }

    /**
     * @return The instance ID of the Chrome window that was accessed last if the maximum number of
     *         instances is open. If fewer than the maximum number is open, the default ID will be
     *         returned, indicative of an unused window ID that can be potentially allocated to
     *         launch a VIEW intent.
     */
    public static int getInstanceIdForViewIntent() {
        int windowId = MultiWindowUtils.INVALID_INSTANCE_ID;
        int maxInstances = MultiWindowUtils.getMaxInstances();
        if (MultiWindowUtils.getInstanceCount() < maxInstances) return windowId;
        for (int i = 0; i < maxInstances; i++) {
            if (MultiWindowUtils.readLastAccessedTime(i)
                    > MultiWindowUtils.readLastAccessedTime(windowId)) {
                windowId = i;
            }
        }
        return windowId;
    }
}
