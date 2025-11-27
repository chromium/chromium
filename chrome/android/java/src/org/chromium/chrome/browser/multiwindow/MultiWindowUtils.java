// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static android.os.Build.VERSION.SDK_INT;
import static android.os.Build.VERSION.SDK_INT_FULL;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManagerApi31.getInstanceCountForManageWindowsMenu;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Build.VERSION_CODES_FULL;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SupportedProfileType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistenceUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.function.Supplier;

/**
 * Utilities for detecting multi-window/multi-instance support.
 *
 * <p>Thread-safe: This class may be accessed from any thread.
 */
@NullMarked
public class MultiWindowUtils implements ActivityStateListener {
    public static final int INVALID_TASK_ID = MultiInstanceManager.INVALID_TASK_ID;
    private static final int DEFAULT_TAB_COUNT_FOR_RELAUNCH = 0;

    static final String HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumActivities.DesktopWindow";
    static final String HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumInstances.DesktopWindow";
    static final String HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW_INCOGNITO =
            "Android.MultiInstance.NumActivities.DesktopWindow.Incognito";
    static final String HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW_INCOGNITO =
            "Android.MultiInstance.NumInstances.DesktopWindow.Incognito";
    static final String OPEN_ADJACENTLY_PARAM = "open_adjacently";

    private static MultiWindowUtils sInstance = new MultiWindowUtils();
    protected static @Nullable Supplier<Activity> sActivitySupplierForTesting;

    private static @Nullable Integer sMaxInstancesForTesting;
    private static @Nullable Integer sInstanceCountForTesting;
    private static @Nullable Boolean sMultiInstanceApi31EnabledForTesting;
    private final boolean mMultiInstanceApi31Enabled;
    private static @Nullable Boolean sIsMultiInstanceApi31Enabled;


    // Used to keep track of whether ChromeTabbedActivity2 is running. A tri-state Boolean is
    // used in case both activities die in the background and MultiWindowUtils is recreated.
    private @Nullable Boolean mTabbedActivity2TaskRunning;
    private @Nullable WeakReference<ChromeTabbedActivity> mLastResumedTabbedActivity;
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
     * @return Whether the new launch mode 'singleInstancePerTask' is configured to allow multiple
     *     instantiation of Chrome instance.
     */
    public static boolean isMultiInstanceApi31Enabled() {
        if (sMultiInstanceApi31EnabledForTesting != null) {
            return sMultiInstanceApi31EnabledForTesting;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return false;
        if (ChromeFeatureList.sCacheIsMultiInstanceApi31Enabled.isEnabled()
                && sIsMultiInstanceApi31Enabled != null) {
            return sIsMultiInstanceApi31Enabled;
        }

        Context context = ContextUtils.getApplicationContext();
        String packageName = context.getPackageName();
        String className = ChromeTabbedActivity.class.getCanonicalName();
        ComponentName comp = new ComponentName(packageName, className);
        try {
            int launchMode = context.getPackageManager().getActivityInfo(comp, 0).launchMode;
            boolean isSingleInstancePerTaskConfigured =
                    launchMode == ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
            sIsMultiInstanceApi31Enabled = isSingleInstancePerTaskConfigured;
            return isSingleInstancePerTaskConfigured;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /**
     * @return The maximum number of instances that a user is allowed to create.
     */
    public static int getMaxInstances() {
        if (sMaxInstancesForTesting != null) {
            return sMaxInstancesForTesting;
        }

        if (!isMultiInstanceApi31Enabled()) {
            return TabWindowManager.MAX_SELECTORS_LEGACY;
        }

        if (!ChromeFeatureList.sDisableInstanceLimit.isEnabled()) {
            return TabWindowManager.MAX_SELECTORS_S;
        }

        if (DeviceInfo.isDesktop()) {
            return TabWindowManager.MAX_SELECTORS;
        }

        int memoryThresholdMb = ChromeFeatureList.sDisableInstanceLimitMemoryThresholdMb.getValue();
        boolean isAboveMemoryThreshold =
                SysUtils.amountOfPhysicalMemoryKB()
                        >= memoryThresholdMb * ConversionUtils.KILOBYTES_PER_MEGABYTE;
        if (isAboveMemoryThreshold) {
            return ChromeFeatureList.sDisableInstanceLimitMaxCount.getValue();
        }
        return TabWindowManager.MAX_SELECTORS_S;
    }

    /** Returns the singleton instance of MultiWindowUtils. */
    public static MultiWindowUtils getInstance() {
        return sInstance;
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in Android N+ multi-window mode.
     */
    public boolean isInMultiWindowMode(@Nullable Activity activity) {
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
    public boolean isOpenInOtherWindowSupported(@Nullable Activity activity) {
        if (activity == null) return false;
        if (!isInMultiWindowMode(activity) && !isInMultiDisplayMode(activity)) return false;
        // Automotive is currently restricted to a single window.
        if (DeviceInfo.isAutomotive()) return false;

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && activity instanceof ChromeTabbedActivity) {
            @SupportedProfileType
            int supportedProfileType = ((ChromeTabbedActivity) activity).getSupportedProfileType();
            int instanceCount =
                    supportedProfileType == SupportedProfileType.OFF_THE_RECORD
                            ? getIncognitoInstanceCount(/* activeOnly= */ true)
                            : getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
            return instanceCount > 1;
        }

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
        if (DeviceInfo.isAutomotive()) return false;

        // Do not allow move for last tab when homepage enabled and is set to a custom url.
        if (hasAtMostOneTabWithHomepageEnabled(tabModelSelector)) {
            return false;
        }
        if (instanceSwitcherEnabled() && isMultiInstanceApi31Enabled()) {
            // Moving tabs should be possible to any other instance.
            return getInstanceCountWithFallback(PersistedInstanceType.ANY) > 1;
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
     * @param tabModelSelector Used to pull total tab count and selected tab count.
     * @return whether it is last tab with homepage enabled and set to an custom url.
     */
    public boolean hasAllTabsSelectedWithHomepageEnabled(TabModelSelector tabModelSelector) {
        boolean hasAllTabsSelected =
                tabModelSelector.getTotalTabCount()
                        <= tabModelSelector.getCurrentModel().getMultiSelectedTabsCount();

        // Chrome app is set to close with zero tabs when homepage is enabled and set to a custom
        // url other than the NTP. We should not allow dragging the last tab or display 'Move to
        // other window' in this scenario as the source window might be closed before drag n drop
        // completes properly and thus cause other complications.
        boolean shouldAppCloseWithZeroTabs =
                HomepageManager.getInstance().shouldCloseAppWithZeroTabs();
        return hasAllTabsSelected && shouldAppCloseWithZeroTabs;
    }

    /**
     * @param tabModelSelector Used to pull total tab count.
     * @param tabGroupModelFilter Used to pull tab group info.
     * @return whether it is last tab group with homepage enabled and set to an custom url.
     */
    public boolean hasAtMostOneTabGroupWithHomepageEnabled(
            TabModelSelector tabModelSelector, TabGroupModelFilter tabGroupModelFilter) {
        int numOfTabs = tabModelSelector.getTotalTabCount();
        Tab firstTab = tabModelSelector.getCurrentTabModelSupplier().get().getTabAt(0);
        if (firstTab == null) return true;
        int numOfTabsInGroup = tabGroupModelFilter.getTabCountForGroup(firstTab.getTabGroupId());

        // Chrome app is set to close with zero tabs when homepage is enabled and set to a custom
        // url other than the NTP. We should not allow dragging the last tab group in this scenario
        // as the source window might be closed before drag n drop completes properly and thus cause
        // other complications.
        boolean shouldAppCloseWithZeroTabs =
                HomepageManager.getInstance().shouldCloseAppWithZeroTabs();
        return numOfTabs == numOfTabsInGroup && shouldAppCloseWithZeroTabs;
    }

    /**
     * See if Chrome can get itself into multi-window mode.
     *
     * @return {@code True} if Chrome can get itself into multi-window mode.
     */
    public boolean canEnterMultiWindowMode() {
        // Automotive is currently restricted to a single window.
        if (DeviceInfo.isAutomotive()) return false;

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
    public @Nullable Class<? extends Activity> getOpenInOtherWindowActivity(
            @Nullable Activity current) {
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
     * @param source The source of the new window intent.
     * @return The created intent.
     */
    public static Intent createNewWindowIntent(
            Context context,
            int instanceId,
            boolean preferNew,
            boolean openAdjacently,
            boolean addTrustedIntentExtras,
            @NewWindowAppSource int source) {
        assert isMultiInstanceApi31Enabled();
        Intent intent = new Intent(context, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        if (instanceId != INVALID_WINDOW_ID) {
            intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, instanceId);
        }
        if (preferNew) intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        if (openAdjacently) intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        if (addTrustedIntentExtras) {
            IntentUtils.addTrustedIntentExtras(intent);
        }
        RecordHistogram.recordEnumeratedHistogram(
                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                source,
                NewWindowAppSource.NUM_ENTRIES);
        return intent;
    }

    /**
     * @param intent The {@link Intent} to determine whether creation of a new instance is
     *     preferred.
     * @return {@code true} if creation of a new instance from {@code intent} is preferred, {@code
     *     false} otherwise.
     */
    public static boolean getExtraPreferNewFromIntent(Intent intent) {
        // Default to creating a new instance when FLAG_ACTIVITY_MULTIPLE_TASK is set. This is
        // required to fulfill new window creation requests initiated for MAIN intents from the OS.
        // This logic is gated behind the OS version that includes a fix that this assumption is
        // dependent on, see crbug.com/436477060 for more details.
        int flags = intent.getFlags();
        boolean preferNew = false;
        if (SDK_INT >= VERSION_CODES.BAKLAVA) {
            preferNew =
                    SDK_INT_FULL > VERSION_CODES_FULL.BAKLAVA
                            && Intent.ACTION_MAIN.equals(intent.getAction())
                            && (flags & Intent.FLAG_ACTIVITY_MULTIPLE_TASK) != 0
                            && (flags & Intent.FLAG_ACTIVITY_NEW_TASK) != 0;
        }
        return IntentUtils.safeGetBooleanExtra(intent, IntentHandler.EXTRA_PREFER_NEW, preferNew);
    }

    /**
     * Returns the number of restorable Chrome instances of a given type.
     *
     * @param type The {@link PersistedInstanceType} of instances to count.
     * @return The number of restorable Chrome instances; an instance is considered restorable if it
     *     has tabs or is associated with a live task. If Robust Window Management is not enabled,
     *     the type is ignored and all instances, both active and inactive, are counted.
     */
    // TODO (crbug.com/456833895): Remove restorable instance check post-launch.
    public static int getInstanceCountWithFallback(@PersistedInstanceType int type) {
        if (sInstanceCountForTesting != null) {
            return sInstanceCountForTesting;
        }
        if (!UiUtils.isRobustWindowManagementEnabled()) {
            type = PersistedInstanceType.ANY;
        }
        Set<Integer> ids = MultiInstanceManagerApi31.getPersistedInstanceIds(type);
        int count = 0;
        for (Integer id : ids) {
            if (isRestorableInstance(id)) {
                count++;
            }
        }
        return count;
    }

    /**
     * Returns the number of restorable incognito instances.
     *
     * <p>This is a temporary method created to allow incognito window (IW) features to count
     * incognito instances without being blocked by the {@code RobustWindowManagement} flag check in
     * {@link #getInstanceCountWithFallback()}. This method should be removed after the Robust
     * Window Management feature is fully launched, and callers should be migrated to a generic
     * instance counting method.
     *
     * @param activeOnly If true, only counts active incognito instances. Otherwise, counts all
     *     incognito instances (both active and inactive).
     * @return The number of restorable incognito instances matching the criteria.
     */
    // TODO (crbug.com/461553972): Remove this method after Robust Window Management is launched.
    public static int getIncognitoInstanceCount(boolean activeOnly) {
        int instanceType = PersistedInstanceType.OFF_THE_RECORD;
        if (activeOnly) {
            instanceType |= PersistedInstanceType.ACTIVE;
        }
        Set<Integer> ids = MultiInstanceManagerApi31.getPersistedInstanceIds(instanceType);
        int count = 0;
        for (Integer id : ids) {
            if (isRestorableInstance(id)) {
                count++;
            }
        }
        return count;
    }

    /**
     * @return Whether the app menu 'Manage windows' should be shown.
     */
    public static boolean shouldShowManageWindowsMenu() {
        return getInstanceCountForManageWindowsMenu() > 1;
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
     * @param current Current activity trying to find another foreground activity.
     * @return ChromeTabbedActivity instance of the task that is running in foreground and also
     *     satisfies the profile requirement. {@code null} if there is no such task.
     */
    public static @Nullable Activity getForegroundWindowActivity(Activity current) {
        if (sActivitySupplierForTesting != null) {
            return sActivitySupplierForTesting.get();
        }
        List<Activity> runningActivities = ApplicationStatus.getRunningActivities();
        int currentTaskId = current.getTaskId();
        // The outer loop finds a visible task.
        for (Activity activity : runningActivities) {
            int taskId = activity.getTaskId();
            if (taskId == currentTaskId || !isActivityVisible(activity)) {
                continue;
            }
            // The inner loop finds the ChromeTabbedActivity within the visible task.
            // This ChromeTabbedActivity may not be visible.
            for (Activity a : runningActivities) {
                if (a.getTaskId() == taskId && a instanceof ChromeTabbedActivity) {
                    return a;
                }
            }
        }
        return null;
    }

    /**
     * @param current Current activity trying to find another foreground activity.
     * @param incognito Whether the foreground activity should be incognito profile.
     * @return ChromeTabbedActivity instance of the task that is running in foreground and also
     *     satisfies the profile requirement. {@code null} if there is no such task.
     */
    public static @Nullable Activity getForegroundWindowActivityWithProfileType(
            Activity current, boolean incognito) {
        if (sActivitySupplierForTesting != null) {
            return sActivitySupplierForTesting.get();
        }
        List<Activity> runningActivities = ApplicationStatus.getRunningActivities();
        int currentTaskId = current.getTaskId();
        // The outer loop finds a visible task.
        for (Activity activity : runningActivities) {
            int taskId = activity.getTaskId();
            if (taskId == currentTaskId || !isActivityVisible(activity)) {
                continue;
            }
            // The inner loop finds the ChromeTabbedActivity within the visible task.
            // This ChromeTabbedActivity may not be visible.
            for (Activity a : runningActivities) {
                if (a.getTaskId() == taskId
                        && a instanceof ChromeTabbedActivity cta
                        && isProfileTypeSupported(cta, incognito)) {
                    return a;
                }
            }
        }
        return null;
    }

    private static boolean isProfileTypeSupported(ChromeTabbedActivity cta, boolean incognito) {
        @SupportedProfileType int supportedProfileType = cta.getSupportedProfileType();
        if (incognito) {
            return supportedProfileType == SupportedProfileType.MIXED
                    || supportedProfileType == SupportedProfileType.OFF_THE_RECORD;
        } else {
            return supportedProfileType == SupportedProfileType.MIXED
                    || supportedProfileType == SupportedProfileType.REGULAR;
        }
    }

    /**
     * Determines if multiple instances of Chrome are running.
     *
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
                if (lastResumedClassName.equals(ChromeTabbedActivity.class)) {
                    return ChromeTabbedActivity.class;
                }
                if (lastResumedClassName.equals(ChromeTabbedActivity2.class)) {
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
    public static boolean isActivityVisible(@Nullable Activity activity) {
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
     * @return Whether there is an activity, other than the current one, that is running in the
     *     foreground.
     */
    public boolean isChromeRunningInAdjacentWindow(Activity currentActivity) {
        return getForegroundWindowActivity(currentActivity) != null;
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
     *
     * @param index Instance ID
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void writeLastAccessedTime(int index) {
        ChromeSharedPreferences.getInstance()
                .writeLong(lastAccessedTimeKey(index), TimeUtils.currentTimeMillis());
    }

    @VisibleForTesting
    public @Nullable Boolean getTabbedActivity2TaskRunning() {
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
                        long current = TimeUtils.currentTimeMillis();
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
                        long current = TimeUtils.currentTimeMillis();
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

        new UkmRecorder(tab.getWebContents(), "Android.MultiWindowChangeActivity")
                .addMetric(
                        "ActivityType",
                        isInMultiWindowMode
                                ? MultiWindowActivityType.ENTER
                                : MultiWindowActivityType.EXIT)
                .record();
    }

    /**
     * Records the ukms about if the activity is in multi-window mode when the activity is shown.
     *
     * @param activity The current Context, used to retrieve the ActivityManager system service.
     * @param tab The current activity {@link Tab}.
     */
    public void recordMultiWindowStateUkm(Activity activity, Tab tab) {
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder(tab.getWebContents(), "Android.MultiWindowState")
                .addMetric(
                        "WindowState",
                        isInMultiWindowMode(activity)
                                ? MultiWindowState.MULTI_WINDOW
                                : MultiWindowState.SINGLE_WINDOW)
                .record();
    }

    /**
     * @return The instance ID of the Chrome window with a running activity that was accessed last.
     */
    public static int getInstanceIdForViewIntent() {
        return getLastAccessedWindowIdInternal(/* includeRunningActivitiesOnly= */ true);
    }

    /**
     * @return The instance ID of the Chrome window that was last accessed. This will return {@code
     *     INVALID_WINDOW_ID} if no persisted instance state is found.
     */
    public static int getLastAccessedWindowId() {
        return getLastAccessedWindowIdInternal(/* includeRunningActivitiesOnly= */ false);
    }

    private static int getLastAccessedWindowIdInternal(boolean includeRunningActivitiesOnly) {
        int lastAccessedWindowId = INVALID_WINDOW_ID;
        long maxAccessedTime = 0;

        SparseIntArray windowIdsOfRunningTabbedActivities = null;
        if (includeRunningActivitiesOnly) {
            windowIdsOfRunningTabbedActivities =
                    MultiInstanceManagerApi31.getWindowIdsOfRunningTabbedActivities();
        }

        Set<Integer> persistedIds = MultiInstanceManagerApi31.getAllPersistedInstanceIds();

        for (int id : persistedIds) {
            if (includeRunningActivitiesOnly && windowIdsOfRunningTabbedActivities != null) {
                if (windowIdsOfRunningTabbedActivities.indexOfValue(id) < 0) continue;
            }

            long accessedTime = readLastAccessedTime(id);
            if (accessedTime > maxAccessedTime) {
                maxAccessedTime = accessedTime;
                lastAccessedWindowId = id;
            }
        }
        return lastAccessedWindowId;
    }

    /**
     * Determines whether a new window should be opened adjacently or in full screen. This relies on
     * an experimental param set on the server-side, with behavior defaulting to adjacent launch.
     *
     * @return {@code false} when a new window should be opened in full screen, {@code true}
     *     otherwise.
     */
    public static boolean shouldOpenInAdjacentWindow() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                OPEN_ADJACENTLY_PARAM,
                true);
    }

    /**
     * Launch the given intent in an existing ChromeTabbedActivity instance.
     *
     * @param intent The intent to launch.
     * @param instanceId ID of the instance to launch the intent in.
     */
    public static void launchIntentInInstance(Intent intent, int instanceId) {
        MultiInstanceManagerApi31.launchIntentInExistingActivity(intent, instanceId);
    }

    /**
     * Launch an intent in another window. It it unknown to our caller if the other window currently
     * exists in recent apps or not. This method will attempt to discern this and take the
     * appropriate action.
     *
     * @param context The context used to launch the intent.
     * @param intent The intent to launch.
     * @param windowId The id to identify the target window/activity.
     */
    public static void launchIntentInMaybeClosedWindow(
            Context context, Intent intent, @WindowId int windowId) {
        MultiInstanceManagerApi31.launchIntentInUnknown(context, intent, windowId);
    }

    /**
     * @param activity The {@link Activity} associated with the current context.
     * @return The instance ID of the Chrome window where the link intent will be launched.
     *     INVALID_WINDOW_ID will be returned if fewer than the maximum number of active instances
     *     are open. The instance ID associated with the specified, valid activity will be returned
     *     if the maximum number of active instances is open.
     */
    public static int getInstanceIdForLinkIntent(Activity activity) {
        // INVALID_WINDOW_ID indicates that a new instance will be used to launch the link intent.
        int instanceCount =
                getInstanceCountWithFallback(
                        MultiInstanceManagerApi31.PersistedInstanceType.ACTIVE);
        if (instanceCount < getMaxInstances()) return INVALID_WINDOW_ID;
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);
        assert windowId != INVALID_WINDOW_ID
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
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @InstanceAllocationType int instanceAllocationType,
            boolean isColdStart) {
        // Emit the histogram only for an activity that starts in a desktop window.
        if (!AppHeaderUtils.isAppInDesktopWindow(desktopWindowStateManager)) return;

        // Emit the histogram only for a newly created activity that is cold-started.
        if (!isColdStart) return;

        // Emit histograms for running activity count.
        RecordHistogram.recordExactLinearHistogram(
                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW,
                MultiInstanceManagerApi31.getRunningTabbedActivityCount(),
                TabWindowManager.MAX_SELECTORS + 1);

        // Emit histograms for total instance count.
        RecordHistogram.recordExactLinearHistogram(
                HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW,
                getInstanceCountWithFallback(PersistedInstanceType.ANY),
                TabWindowManager.MAX_SELECTORS + 1);

        // Emit histograms for running Incognito activity count.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW_INCOGNITO,
                    getIncognitoInstanceCount(/* activeOnly= */ true),
                    TabWindowManager.MAX_SELECTORS + 1);
        }

        // Emit histograms for total Incognito instance count.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW_INCOGNITO,
                    getIncognitoInstanceCount(/* activeOnly= */ false),
                    TabWindowManager.MAX_SELECTORS + 1);
        }
    }

    /**
     * Records count of tabs with shared preference before Chrome is paused and becomes invisible to
     * the user. The value stored is only used for relaunching chrome and it may not be accurate if
     * Chrome remains active in the foreground or background without being terminated.
     *
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param windowId The id of the window.
     */
    public static void recordTabCountForRelaunchWhenActivityPaused(
            TabModelSelector tabModelSelector, int windowId) {
        List<TabModel> models = tabModelSelector.getModels();
        int totalCount = 0;
        for (TabModel model : models) {
            for (Tab tab : model) {
                if (!TabPersistenceUtils.shouldSkipTab(tab)) {
                    totalCount++;
                }
            }
        }

        SharedPreferences.Editor editor = ChromeSharedPreferences.getInstance().getEditor();
        String tabCountForRelaunchKey = getTabCountForRelaunchKey(windowId);
        editor.putInt(tabCountForRelaunchKey, totalCount);
        // The ChromeSharedPreferences.getInstance().writeInt() method uses editor.apply() instead
        // of editor.commit(). The editor.apply() method writes data to memory and returns
        // immediately, while the actual disk write occurs asynchronously in a background thread. On
        // the other hand, editor.commit() writes data directly to disk and waits for the operation
        // to complete. Since apply() is asynchronous, if the program is forcibly closed right after
        // calling it (e.g., in our case where Chrome is closed and then relaunched), the disk write
        // may not finish in time, potentially resulting in data loss. Therefore, editor.commit() is
        // used here to ensure data is reliably saved.
        editor.commit();
    }

    /**
     * Returns the total number of tabs for relaunch across both regular and incognito browsing
     * modes through shared preference key.
     *
     * @param windowId The id of the window.
     */
    public static int getTabCountForRelaunchFromSharedPrefs(int windowId) {
        String tabCountForRelaunchKey = getTabCountForRelaunchKey(windowId);
        return ChromeSharedPreferences.getInstance()
                .readInt(
                        tabCountForRelaunchKey, /* defaultValue= */ DEFAULT_TAB_COUNT_FOR_RELAUNCH);
    }

    /** Returns the tab count for relaunch key. */
    @VisibleForTesting
    static String getTabCountForRelaunchKey(int windowId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.createKey(
                String.valueOf(windowId));
    }

    /**
     * Creates and shows a message to notify a user about instance restoration when the number of
     * persisted instances exceeds the max instance count after an instance limit downgrade. This is
     * relevant only when both active and inactive instances contribute to the instance limit.
     *
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     * @param context The current context.
     * @param primaryActionRunnable The {@link Runnable} that will be executed when the message
     *     primary action button is clicked.
     * @return Whether the message was shown.
     */
    public static boolean maybeShowInstanceRestorationMessage(
            @Nullable MessageDispatcher messageDispatcher,
            Context context,
            Runnable primaryActionRunnable) {
        if (messageDispatcher == null) return false;

        // Show the message only when robust window management is disabled and the number of
        // persisted instances exceeds the instance limit.
        if (UiUtils.isRobustWindowManagementEnabled()
                || getInstanceCountWithFallback(MultiInstanceManagerApi31.PersistedInstanceType.ANY)
                        <= getMaxInstances()) {
            return false;
        }

        // Show the message only if the message is not already shown.
        if (ChromeSharedPreferences.getInstance()
                .readBoolean(
                        ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, false)) {
            return false;
        }

        Resources resources = context.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.MULTI_INSTANCE_RESTORATION_ON_DOWNGRADED_LIMIT)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(
                                        R.string.multi_instance_restoration_message_title,
                                        getMaxInstances()))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(
                                        R.string.multi_instance_restoration_message_description))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_chrome)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.multi_instance_message_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    primaryActionRunnable.run();
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();

        messageDispatcher.enqueueWindowScopedMessage(message, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, true);
        return true;
    }

    /**
     * Creates and shows a message to notify a user that a new window cannot be created because
     * {@link MultiWindowUtils#getMaxInstances()} activities already exist.
     *
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     * @param context The current context.
     * @param primaryActionRunnable The {@link Runnable} that will be executed when the message
     *     primary action button is clicked.
     */
    public static void showInstanceCreationLimitMessage(
            @Nullable MessageDispatcher messageDispatcher,
            Context context,
            Runnable primaryActionRunnable) {
        if (messageDispatcher == null) return;

        Resources resources = context.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.MULTI_INSTANCE_CREATION_LIMIT)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(
                                        R.string.multi_instance_creation_limit_message_title,
                                        getMaxInstances()))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(
                                        R.string.multi_instance_creation_limit_message_description))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_chrome)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.multi_instance_message_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    primaryActionRunnable.run();
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();

        messageDispatcher.enqueueWindowScopedMessage(message, false);
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

    public static void setActivitySupplierForTesting(Supplier<Activity> supplier) {
        sActivitySupplierForTesting = supplier;
        ResettersForTesting.register(() -> sActivitySupplierForTesting = null);
    }
}
