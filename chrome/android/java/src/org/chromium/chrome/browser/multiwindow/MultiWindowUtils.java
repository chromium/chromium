// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityOptions;
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
import android.util.SparseIntArray;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
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
    public static final IntCachedFieldTrialParameter
            BACK_TO_BACK_CTA_CREATION_TIMESTAMP_DIFF_THRESHOLD_MS =
                    ChromeFeatureList.newIntCachedFieldTrialParameter(
                            ChromeFeatureList.TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH,
                            "activity_creation_timestamp_diff_threshold_ms",
                            1000);

    static final String HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumActivities.DesktopWindow";
    static final String HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumInstances.DesktopWindow";
    static final String HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX = ".NewInstance";
    static final String HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX =
            ".ExistingInstance";

    private static MultiWindowUtils sInstance = new MultiWindowUtils();

    private static Integer sMaxInstancesForTesting;
    private static Integer sInstanceCountForTesting;

    private final boolean mMultiInstanceApi31Enabled;
    private static Boolean sMultiInstanceApi31EnabledForTesting;

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

    @IntDef({
        InstanceAllocationType.DEFAULT,
        InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK,
        InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK,
        InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK,
        InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE,
        InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
        InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK,
        InstanceAllocationType.INVALID_INSTANCE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InstanceAllocationType {
        int DEFAULT = 0;
        int EXISTING_INSTANCE_UNMAPPED_TASK = 1;
        int EXISTING_INSTANCE_MAPPED_TASK = 2;
        int PREFER_NEW_INSTANCE_NEW_TASK = 3;
        int PREFER_NEW_INVALID_INSTANCE = 4;
        int NEW_INSTANCE_NEW_TASK = 5;
        int EXISTING_INSTANCE_NEW_TASK = 6;
        int INVALID_INSTANCE = 7;
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
        return true;
    }

    /**
     * @return Whether the new launch mode 'singleInstancePerTask' is configured to allow
     *         multiple instantiation of Chrome instance.
     */
    public static boolean isMultiInstanceApi31Enabled() {
        if (sMultiInstanceApi31EnabledForTesting != null) {
            return sMultiInstanceApi31EnabledForTesting;
        }
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
        return sMaxInstancesForTesting != null
                ? sMaxInstancesForTesting
                : (isMultiInstanceApi31Enabled()
                        ? (ChromeFeatureList.sDisableInstanceLimit.isEnabled()
                                ? TabWindowManager.MAX_SELECTORS
                                : TabWindowManager.MAX_SELECTORS_S)
                        : TabWindowManager.MAX_SELECTORS_LEGACY);
    }

    /** Returns the singleton instance of MultiWindowUtils. */
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

        return activity.isInMultiWindowMode();
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether the system currently supports multiple displays, requiring Android Q+.
     */
    public boolean isInMultiDisplayMode(Activity activity) {
        // TODO(crbug.com/41378391): Consider supporting more displays.
        return ApiCompatibilityUtils.getTargetableDisplayIds(activity).size() == 2;
    }

    public void setIsInMultiWindowModeForTesting(boolean isInMultiWindowMode) {
        mIsInMultiWindowModeForTesting = isInMultiWindowMode;
        ResettersForTesting.register(() -> mIsInMultiWindowModeForTesting = false);
    }

    /** Returns whether the given activity currently supports opening tabs to the other window. */
    public boolean isOpenInOtherWindowSupported(Activity activity) {
        if (!isInMultiWindowMode(activity) && !isInMultiDisplayMode(activity)) return false;
        // Automotive is currently restricted to a single window.
        if (BuildInfo.getInstance().isAutomotive) return false;

        return getOpenInOtherWindowActivity(activity) != null;
    }

    /**
     * @param activity that is initiating tab move.
     * @param tabModelSelector {@link TabModelSelector} to get total tab count. Returns whether the
     *     given activity currently supports moving tabs to the other window.
     */
    public boolean isMoveToOtherWindowSupported(
            Activity activity, TabModelSelector tabModelSelector) {
        // Not supported on automotive devices.
        if (BuildInfo.getInstance().isAutomotive) return false;

        // Do not allow move for last tab when homepage enabled and is set to a custom url.
        if (hasAtMostOneTabWithHomepageEnabled(tabModelSelector)) {
            return false;
        }
        if (instanceSwitcherEnabled() && isMultiInstanceApi31Enabled()) {
            // Moving tabs should be possible to any other instance.
            return getInstanceCount() > 1;
        } else {
            return isOpenInOtherWindowSupported(activity);
        }
    }

    /**
     * @param tabModelSelector Used to pull total tab count.
     * @return whether it is last tab with homepage enabled and set to an custom url.
     */
    public boolean hasAtMostOneTabWithHomepageEnabled(TabModelSelector tabModelSelector) {
        boolean hasAtMostOneTab = tabModelSelector.getTotalTabCount() <= 1;

        // Chrome app is set to close with zero tabs when homepage is enabled and set to a custom
        // url other than the NTP. We should not allow dragging the last tab or display 'Move to
        // other window' in this scenario as the source window might be closed before drag n drop
        // completes properly and thus cause other complications.
        boolean shouldAppCloseWithZeroTabs =
                HomepageManager.getInstance().shouldCloseAppWithZeroTabs();
        return hasAtMostOneTab && shouldAppCloseWithZeroTabs;
    }

    /**
     * See if Chrome can get itself into multi-window mode.
     * @param activity The {@link Activity} to check.
     * @return {@code True} if Chrome can get itself into multi-window mode.
     */
    public boolean canEnterMultiWindowMode(Activity activity) {
        // Automotive is currently restricted to a single window.
        if (BuildInfo.getInstance().isAutomotive) return false;

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
        if (!isMultiInstanceApi31Enabled()
                && targetActivity.equals(ChromeTabbedActivity.class)
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
     * @param addTrustedIntentExtras (@code true} if the TRUSTED_APPLICATION_CODE_EXTRA will be
     *         added to the intent to identify it as coming from a trusted source. This should be
     *         set to 'false' if the Intent could be received by an app besides Chrome (e.g. when
     *         attaching to ClipData for a drag event).
     * @return The created intent.
     */
    public static Intent createNewWindowIntent(
            Context context,
            int instanceId,
            boolean preferNew,
            boolean openAdjacently,
            boolean addTrustedIntentExtras) {
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
        if (addTrustedIntentExtras) {
            IntentUtils.addTrustedIntentExtras(intent);
        }
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
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(id);
        return options.toBundle();
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
        if (sInstanceCountForTesting != null) return sInstanceCountForTesting;
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
                StringBuilder activityNameBuilder = new StringBuilder();
                for (Activity a : runningActivities) {
                    if (a.getTaskId() == taskId) {
                        activityNameBuilder.append(a.getClass().getName()).append(",");
                        if (a instanceof ChromeTabbedActivity) return a;
                    }
                }
                assert false
                        : "Should have found the ChromeTabbedActivity of the visible task."
                                + " Activities in this task: "
                                + activityNameBuilder;
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
        boolean tabbed2TaskRunning =
                isActivityTaskInRecents(ChromeTabbedActivity2.class.getName(), context);

        // Exit early if ChromeTabbedActivity2 isn't running.
        if (!tabbed2TaskRunning) {
            mTabbedActivity2TaskRunning = false;
            return ChromeTabbedActivity.class;
        }

        boolean tabbedTaskRunning =
                isActivityTaskInRecents(ChromeTabbedActivity.class.getName(), context);
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
                if (tabbedTaskRunning && lastResumedClassName.equals(ChromeTabbedActivity.class)) {
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
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
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
     * @return A map taskID : boolean containing the visible tasks.
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
        return ChromeSharedPreferences.getInstance().readLong(lastAccessedTimeKey(index));
    }

    /**
     * Write the time this instance is accessed.
     * @param index Instance ID
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void writeLastAccessedTime(int index) {
        ChromeSharedPreferences.getInstance()
                .writeLong(lastAccessedTimeKey(index), System.currentTimeMillis());
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
    public void recordMultiWindowModeChanged(
            boolean isInMultiWindowMode,
            boolean isDeferredStartup,
            boolean isFirstActivity,
            @Nullable Tab tab) {
        if (isFirstActivity) {
            if (isInMultiWindowMode) {
                if (mMultiInstanceApi31Enabled) {
                    SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
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
                    SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
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

        new UkmRecorder.Bridge()
                .recordEventWithIntegerMetric(
                        tab.getWebContents(),
                        "Android.MultiWindowChangeActivity",
                        "ActivityType",
                        isInMultiWindowMode
                                ? MultiWindowActivityType.ENTER
                                : MultiWindowActivityType.EXIT);
    }

    /**
     * Records the ukms about if the activity is in multi-window mode when the activity is shown.
     * @param activity The current Context, used to retrieve the ActivityManager system service.
     * @param tab The current activity {@link Tab}.
     */
    public void recordMultiWindowStateUkm(Activity activity, Tab tab) {
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge()
                .recordEventWithIntegerMetric(
                        tab.getWebContents(),
                        "Android.MultiWindowState",
                        "WindowState",
                        isInMultiWindowMode(activity)
                                ? MultiWindowState.MULTI_WINDOW
                                : MultiWindowState.SINGLE_WINDOW);
    }

    /**
     * @param preferNew Whether a new instance is preferred to launch a VIEW intent. {@code true} if
     *     a new instance is preferred, {@code false} if an existing instance is preferred.
     * @return The instance ID of the Chrome window with a running activity that was accessed last,
     *     if an existing instance is preferred to launch the intent, or if the maximum number of
     *     instances is open. If fewer than the maximum number is open, the default ID will be
     *     returned if |preferNew| is true, indicative of an unused window ID that can be allocated
     *     to the new instance launched by the intent.
     */
    public static int getInstanceIdForViewIntent(boolean preferNew) {
        int windowId = MultiWindowUtils.INVALID_INSTANCE_ID;
        int maxInstances = MultiWindowUtils.getMaxInstances();
        if (preferNew && MultiWindowUtils.getInstanceCount() < maxInstances) return windowId;

        SparseIntArray windowIdsOfRunningTabbedActivities =
                MultiInstanceManagerApi31.getWindowIdsOfRunningTabbedActivities();
        for (int i = 0; i < maxInstances; i++) {
            // Exclude instance IDs of non-running activities.
            if (windowIdsOfRunningTabbedActivities.indexOfValue(i) < 0) continue;
            if (MultiWindowUtils.readLastAccessedTime(i)
                    > MultiWindowUtils.readLastAccessedTime(windowId)) {
                windowId = i;
            }
        }
        return windowId;
    }

    /**
     * Launch the given intent in an existing ChromeTabbedActivity instance.
     * @param intent The intent to launch.
     * @param instanceId ID of the instance to launch the intent in.
     */
    public static void launchIntentInInstance(Intent intent, int instanceId) {
        MultiInstanceManagerApi31.launchIntentInInstance(intent, instanceId);
    }

    /**
     * @param activity The {@link Activity} associated with the current context.
     * @return The instance ID of the Chrome window where the link intent will be launched.
     *     INVALID_INSTANCE_ID will be returned if fewer than the maximum number of instances are
     *     open. The instance ID associated with the specified, valid activity will be returned if
     *     the maximum number of instances is open.
     */
    public static int getInstanceIdForLinkIntent(Activity activity) {
        // INVALID_INSTANCE_ID indicates that a new instance will be used to launch the link intent.
        if (getInstanceCount() < getMaxInstances()) return INVALID_INSTANCE_ID;
        int windowId = TabWindowManagerSingleton.getInstance().getIndexForWindow(activity);
        assert windowId != INVALID_INSTANCE_ID
                : "A valid instance ID was not found for the specified activity.";
        return windowId;
    }

    /**
     * Record the number of running ChromeTabbedActivity's as well as the total number of Chrome
     * instances when a new ChromeTabbedActivity is created in a desktop window.
     *
     * @param instanceAllocationType The {@link InstanceAllocationType} for the new activity.
     * @param isColdStart Whether app startup is a cold start.
     */
    public static void maybeRecordDesktopWindowCountHistograms(
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider,
            @InstanceAllocationType int instanceAllocationType,
            boolean isColdStart) {
        // Emit the histogram only for an activity that starts in a desktop window.
        if (!AppHeaderUtils.isAppInDesktopWindow(desktopWindowStateProvider)) return;

        // Emit the histogram only for a newly created activity that is cold-started.
        if (!isColdStart) return;

        // Emit histograms for running activity count.
        recordDesktopWindowCountHistograms(
                instanceAllocationType,
                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW,
                MultiInstanceManagerApi31.getRunningTabbedActivityCount());

        // Emit histograms for total instance count.
        recordDesktopWindowCountHistograms(
                instanceAllocationType, HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, getInstanceCount());
    }

    private static void recordDesktopWindowCountHistograms(
            @InstanceAllocationType int instanceAllocationType, String histogramName, int count) {
        // Emit generic histogram, irrespective of instance allocation type.
        RecordHistogram.recordExactLinearHistogram(histogramName, count, getMaxInstances() + 1);

        // Emit histogram variant based on instance allocation type.
        String histogramSuffix = HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX;
        if (instanceAllocationType != InstanceAllocationType.NEW_INSTANCE_NEW_TASK
                && instanceAllocationType != InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK) {
            histogramSuffix = HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX;
        }

        RecordHistogram.recordExactLinearHistogram(
                histogramName + histogramSuffix, count, getMaxInstances() + 1);
    }

    public static void setInstanceForTesting(MultiWindowUtils instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    public static void setInstanceCountForTesting(int instanceCount) {
        sInstanceCountForTesting = instanceCount;
        ResettersForTesting.register(() -> sInstanceCountForTesting = null);
    }

    public static void setMaxInstancesForTesting(int maxInstances) {
        sMaxInstancesForTesting = maxInstances;
        ResettersForTesting.register(() -> sMaxInstancesForTesting = null);
    }

    public static void setMultiInstanceApi31EnabledForTesting(boolean value) {
        sMultiInstanceApi31EnabledForTesting = value;
        ResettersForTesting.register(() -> sMultiInstanceApi31EnabledForTesting = null);
    }
}
