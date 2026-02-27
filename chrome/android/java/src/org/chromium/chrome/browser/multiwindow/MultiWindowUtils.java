// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static android.os.Build.VERSION.SDK_INT;
import static android.os.Build.VERSION.SDK_INT_FULL;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Build.VERSION_CODES_FULL;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.AconfigFlaggedApiDelegate;
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
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.InstanceAllocationType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
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
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
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

    private static final int HIGH_INSTANCE_LIMIT_MEMORY_THRESHOLD_MB = 6500;
    public static final String PERSISTENT_STATE_ID = "persistent_state_id";

    static final String HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumActivities.DesktopWindow";
    static final String HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW =
            "Android.MultiInstance.NumInstances.DesktopWindow";
    static final String HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW_INCOGNITO =
            "Android.MultiInstance.NumActivities.DesktopWindow.Incognito";
    static final String HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW_INCOGNITO =
            "Android.MultiInstance.NumInstances.DesktopWindow.Incognito";
    static final String HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION =
            "Android.MultiInstance.PersistAcrossReboots.IdVerification";
    static final String OPEN_ADJACENTLY_PARAM = "open_adjacently";

    static @Nullable Integer sMaxInstancesForTesting;

    private static MultiWindowUtils sInstance = new MultiWindowUtils();
    protected static @Nullable Supplier<Activity> sActivitySupplierForTesting;

    private static @Nullable Integer sIncognitoInstanceCountForTesting;
    private static @Nullable Integer sInstanceCountForTesting;
    private static @Nullable Boolean sMultiInstanceApi31EnabledForTesting;
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

    // LINT.IfChange(persistent_state_id_verification)
    @IntDef({
        PersistentStateIdVerification.NO_PERSISTENT_STATE_NOR_ID,
        PersistentStateIdVerification.MISSING_PERSISTENT_STATE,
        PersistentStateIdVerification.MISSING_PERSISTENT_STATE_ID,
        PersistentStateIdVerification.PERSISTENT_STATE_MATCH,
        PersistentStateIdVerification.PERSISTENT_STATE_MISMATCH,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PersistentStateIdVerification {
        // These values are used for UMA. Don't reuse or reorder values.
        // If you add something, update NUM_ENTRIES.
        int NO_PERSISTENT_STATE_NOR_ID = 0;
        int MISSING_PERSISTENT_STATE = 1;
        int MISSING_PERSISTENT_STATE_ID = 2;
        int PERSISTENT_STATE_MATCH = 3;
        int PERSISTENT_STATE_MISMATCH = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:persistent_state_id_verification)

    protected MultiWindowUtils() {}

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
            ActivityInfo info = context.getPackageManager().getActivityInfo(comp, 0);
            int launchMode = info == null ? ActivityInfo.LAUNCH_MULTIPLE : info.launchMode;
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

        if (DeviceInfo.isDesktop()) {
            return TabWindowManager.MAX_SELECTORS_1000;
        }

        boolean isAboveMemoryThreshold =
                SysUtils.amountOfPhysicalMemoryKB()
                        >= HIGH_INSTANCE_LIMIT_MEMORY_THRESHOLD_MB
                                * ConversionUtils.KILOBYTES_PER_MEGABYTE;
        if (isAboveMemoryThreshold) {
            return TabWindowManager.MAX_SELECTORS_20;
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
            @PersistedInstanceType int instanceType = PersistedInstanceType.ACTIVE;
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                if (tabModelSelector.isIncognitoBrandedModelSelected()) {
                    return getIncognitoInstanceCount(/* activeOnly= */ true) > 1;
                } else {
                    instanceType |= PersistedInstanceType.REGULAR;
                }
            }
            return getInstanceCountWithFallback(instanceType) > 1;
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
        Tab firstTab =
                assumeNonNull(tabModelSelector.getCurrentTabModelSupplier().get()).getTabAt(0);
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
        if (isMultiInstanceApi31Enabled()) return ChromeTabbedActivity.class;

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
     * Creates a new ChromeTabbedActivity to replace a ChromeTabbedActivity2 that survived an
     * upgrade from Android R.
     *
     * @param context The application context of the activity firing the intent.
     * @param windowId The id of the instance for which a new activity will be created. Set this to
     *     {@code #INVALID_WINDOW_ID} to create a brand new window.
     * @param startActivityOptions The {@link Bundle} that will be used to start the activity.
     */
    public static void relaunchChromeTabbedActivity2(
            Context context, int windowId, Bundle startActivityOptions) {
        Intent intent =
                createNewWindowIntent(
                        context,
                        windowId,
                        /* preferNew= */ windowId == INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        NewWindowAppSource.ANDROID_S_UPDATE);
        context.startActivity(intent, startActivityOptions);
    }

    /* package */ static Intent createNewWindowIntent(
            Context context,
            int windowId,
            boolean preferNew,
            boolean openAdjacently,
            @NewWindowAppSource int source) {
        assert isMultiInstanceApi31Enabled();
        Intent intent = new Intent(context, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        if (windowId != INVALID_WINDOW_ID) {
            intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, windowId);
        }
        if (preferNew) intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        if (openAdjacently) intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, source);
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
     * Returns the number of restorable Chrome instances of a given type that are not marked for
     * deletion.
     *
     * @param type The {@link PersistedInstanceType} of instances to count.
     * @return The number of restorable Chrome instances not marked for deletion; an instance is
     *     considered restorable if it has tabs or is associated with a live task. An instance
     *     marked for deletion is restorable, but not usable unless restored. If Robust Window
     *     Management is not enabled, the type is ignored and all instances, both active and
     *     inactive, are counted.
     */
    // TODO (crbug.com/456833895): Remove restorable instance check post-launch.
    public static int getInstanceCountWithFallback(@PersistedInstanceType int type) {
        if (sInstanceCountForTesting != null) {
            return sInstanceCountForTesting;
        }

        if (!isMultiInstanceApi31Enabled()) return 0;

        if (!UiUtils.isRobustWindowManagementEnabled()) {
            type = PersistedInstanceType.ANY;
        }
        Set<Integer> ids = MultiInstanceManagerApi31.getPersistedInstanceIds(type);
        int count = 0;
        for (Integer id : ids) {
            if (isRestorableInstance(id)
                    && !MultiInstancePersistentStore.readMarkedForDeletion(id)) {
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
        if (sIncognitoInstanceCountForTesting != null) {
            return sIncognitoInstanceCountForTesting;
        }

        if (!isMultiInstanceApi31Enabled()) return 0;

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
        return getInstanceCountWithFallback(PersistedInstanceType.ANY) > 1;
    }

    static boolean isRestorableInstance(int index) {
        return MultiInstancePersistentStore.readNormalTabCount(index) != 0
                || (IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && MultiInstancePersistentStore.readIncognitoTabCount(index) != 0)
                || MultiInstancePersistentStore.readTaskId(index) != INVALID_TASK_ID;
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
     * @param instanceId The id of the instance.
     * @return The {@link SupportedProfileType} of the instance.
     */
    public static @SupportedProfileType int readProfileType(int instanceId) {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return MultiInstancePersistentStore.readProfileType(instanceId);
        } else {
            return SupportedProfileType.MIXED;
        }
    }

    /**
     * Verifies that the persistent state passed in Activity creation matches the persistent state
     * associated with the current instance. This is to verify that the OS supplied the correct
     * state, and not an outdated bundle.
     *
     * @param instanceId The id of the instance.
     * @param persistentState The {@link PersistableBundle} passed to the instance in #onCreate().
     */
    public static void verifyLatestPersistentStateId(
            int instanceId, @Nullable PersistableBundle persistentState) {
        boolean containsPersistentStateId =
                MultiInstancePersistentStore.containsLatestPersistentStateId(instanceId);
        int latestPersistentStateId =
                MultiInstancePersistentStore.readLatestPersistentStateId(instanceId);
        if (persistentState == null || instanceId == INVALID_WINDOW_ID) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                    containsPersistentStateId
                            ? PersistentStateIdVerification.MISSING_PERSISTENT_STATE
                            : PersistentStateIdVerification.NO_PERSISTENT_STATE_NOR_ID,
                    PersistentStateIdVerification.NUM_ENTRIES);
            return;
        }

        if (!containsPersistentStateId) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                    PersistentStateIdVerification.MISSING_PERSISTENT_STATE_ID,
                    PersistentStateIdVerification.NUM_ENTRIES);
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                latestPersistentStateId == persistentState.getInt(PERSISTENT_STATE_ID)
                        ? PersistentStateIdVerification.PERSISTENT_STATE_MATCH
                        : PersistentStateIdVerification.PERSISTENT_STATE_MISMATCH,
                PersistentStateIdVerification.NUM_ENTRIES);
    }

    /**
     * @param instanceId The id of the instance.
     * @param latestPersistentStateId The id of the latest {@link PersistableBundle} associated with
     *     this instance.
     */
    public static void writeLatestPersistentStateId(int instanceId, int latestPersistentStateId) {
        MultiInstancePersistentStore.writeLatestPersistentStateId(
                instanceId, latestPersistentStateId);
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
     *
     * @param intent The incoming intent that is starting ChromeTabbedActivity.
     * @param context The current Context, used to retrieve the ActivityManager system service.
     * @return The ChromeTabbedActivity to use for the incoming intent.
     */
    public Class<? extends ChromeTabbedActivity> getTabbedActivityForIntent(
            @Nullable Intent intent, Context context) {
        // 0. Use always ChromeTabbedActivity when multi-instance support in S+ is enabled.
        if (isMultiInstanceApi31Enabled()) return ChromeTabbedActivity.class;

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
     * mode. For second activity, records separate user actions for entering/exiting multi-window
     * mode to avoid recording the same action twice when two instances are running, but still
     * records same UKM since two instances have two different tabs.
     *
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     * @param isDeferredStartup True if the activity is deferred startup.
     * @param isFirstActivity True if the activity is the first activity in multi-window mode.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordMultiWindowModeChanged(
            boolean isInMultiWindowMode,
            boolean isDeferredStartup,
            boolean isFirstActivity,
            @Nullable Tab tab) {
        boolean isMultiInstanceApi31Enabled = isMultiInstanceApi31Enabled();
        if (isFirstActivity) {
            if (isInMultiWindowMode) {
                if (isMultiInstanceApi31Enabled) {
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
                if (isMultiInstanceApi31Enabled) {
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
        if (!isMultiInstanceApi31Enabled()) return lastAccessedWindowId;

        long maxAccessedTime = 0;

        SparseIntArray windowIdsOfRunningTabbedActivities = null;
        if (includeRunningActivitiesOnly) {
            windowIdsOfRunningTabbedActivities = getWindowIdsOfRunningTabbedActivities();
        }

        Set<Integer> persistedIds = MultiInstanceManagerApi31.getAllPersistedInstanceIds();

        for (int id : persistedIds) {
            if (includeRunningActivitiesOnly) {
                int windowId = assumeNonNull(windowIdsOfRunningTabbedActivities).indexOfValue(id);
                if (windowId < 0) continue;
            }

            long accessedTime = MultiInstancePersistentStore.readLastAccessedTime(id);
            if (accessedTime > maxAccessedTime) {
                maxAccessedTime = accessedTime;
                lastAccessedWindowId = id;
            }
        }
        return lastAccessedWindowId;
    }

    private static SparseIntArray getWindowIdsOfRunningTabbedActivities() {
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        var windowIdsOfRunningTabbedActivities = new SparseIntArray();
        for (Activity activity : activities) {
            if (!(activity instanceof ChromeTabbedActivity)) continue;
            int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);
            windowIdsOfRunningTabbedActivities.put(windowId, windowId);
        }
        return windowIdsOfRunningTabbedActivities;
    }

    /**
     * Determines whether a new window should be opened adjacently or in full screen. This relies on
     * an experimental param set on the server-side, with behavior defaulting to adjacent launch.
     *
     * @param activity The current activity.
     * @return {@code false} when a new window should be opened in full screen, {@code true}
     *     otherwise.
     */
    public static boolean shouldOpenInAdjacentWindow(Activity activity) {
        // Always open adjacently if the current activity is in multi-windowing mode.
        if (activity.isInMultiWindowMode()) return true;
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
     * @return Whether the intent was launched successfully.
     */
    public static boolean launchIntentInInstance(Intent intent, int instanceId) {
        Activity activity = getActivityById(instanceId);
        if (!(activity instanceof ChromeTabbedActivity)) return false;
        int taskId = activity.getTaskId();
        if (taskId == INVALID_TASK_ID) return false;

        // Launch the intent in the existing activity and bring the task to foreground if it is
        // alive. AppTask.startActivity() is used to robustly bring specific tasks to the front,
        // which helps bypass Android's Background Activity Launch (BAL) restrictions when a
        // notification is tapped while the target activity is backgrounded (minimized).
        AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, taskId);
        if (appTask != null) {
            intent.setClass(ContextUtils.getApplicationContext(), activity.getClass());
            if (isMultiInstanceApi31Enabled()) {
                // Remove NEW_TASK to prevent the OS from spawning a duplicate instance,
                // and strictly target the existing activity class.
                intent.removeFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                appTask.startActivity(ContextUtils.getApplicationContext(), intent, null);
            } else {
                // On older Android versions or devices where multi-instance is not enabled, the OS
                // enforces strict singleTask checks on AppTask.startActivity() and throws an
                // exception if the task is not empty. However, since these versions do not support
                // multiple tasks for the same ChromeTabbedActivity class, we can safely fallback
                // to Context.startActivity() with NEW_TASK, which will inherently route to the
                // correct task and still bypass BAL restrictions.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), intent);
            }
            return true;
        }

        // Fallback: If the OS lost the AppTask record but our Activity is still alive,
        // manually inject the intent and attempt a best effort move to front.
        ((ChromeTabbedActivity) activity).onNewIntent(intent);
        ApiCompatibilityUtils.moveTaskToFront(activity, taskId, 0);
        return true;
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
        if (!isMultiInstanceApi31Enabled()) return;
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
        int instanceCount = getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
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
        if (!isMultiInstanceApi31Enabled()) return;

        // Emit the histogram only for an activity that starts in a desktop window.
        if (!AppHeaderUtils.isAppInDesktopWindow(desktopWindowStateManager)) return;

        // Emit the histogram only for a newly created activity that is cold-started.
        if (!isColdStart) return;

        // Emit histograms for running activity count.
        RecordHistogram.recordExactLinearHistogram(
                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW,
                MultiInstanceManagerApi31.getRunningTabbedActivityCount(),
                TabWindowManager.MAX_SELECTORS_1000 + 1);

        // Emit histograms for total instance count.
        RecordHistogram.recordExactLinearHistogram(
                HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW,
                getInstanceCountWithFallback(PersistedInstanceType.ANY),
                TabWindowManager.MAX_SELECTORS_1000 + 1);

        // Emit histograms for running Incognito activity count.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW_INCOGNITO,
                    getIncognitoInstanceCount(/* activeOnly= */ true),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
        }

        // Emit histograms for total Incognito instance count.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW_INCOGNITO,
                    getIncognitoInstanceCount(/* activeOnly= */ false),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
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
        MultiInstancePersistentStore.writeTabCountForRelaunchSync(windowId, totalCount);
    }

    /**
     * Returns the total number of tabs for relaunch across both regular and incognito browsing
     * modes from persisted state.
     *
     * @param windowId The id of the window.
     */
    public static int getTabCountForRelaunchFromPersistentStore(int windowId) {
        return MultiInstancePersistentStore.readTabCountForRelaunch(windowId);
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
        if (messageDispatcher == null || !isMultiInstanceApi31Enabled()) return false;

        // Show the message only when robust window management is disabled and the number of
        // persisted instances exceeds the instance limit.
        if (UiUtils.isRobustWindowManagementEnabled()
                || getInstanceCountWithFallback(PersistedInstanceType.ANY) <= getMaxInstances()) {
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

    /**
     * Moves the activity to the given bounds.
     *
     * @param activity The activity to move.
     * @param bounds The bounds to move the activity to.
     * @return Whether the activity was moved.
     */
    public static boolean moveActivityToBounds(Activity activity, Rect bounds) {
        final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            return false;
        }

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) {
            return false;
        }

        final Pair<DisplayAndroid, Rect> localCoordinates =
                DisplayUtil.convertGlobalDipToLocalPxCoordinates(bounds);
        if (localCoordinates == null) {
            return false;
        }

        final DisplayAndroid display = localCoordinates.first;
        final Rect localBounds = localCoordinates.second;

        delegate.moveTaskTo(
                appTask,
                display.getDisplayId(),
                DisplayUtil.clampWindowToDisplay(localBounds, display));
        return true;
    }

    /* package */ static @Nullable Activity getActivityById(int windowId) {
        if (sActivitySupplierForTesting != null) {
            return sActivitySupplierForTesting.get();
        }
        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (windowId == windowManager.getIdForWindow(activity)) return activity;
        }
        return null;
    }

    public static void setInstanceForTesting(MultiWindowUtils instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    public static void setIncognitoInstanceCountForTesting(int instanceCount) {
        sIncognitoInstanceCountForTesting = instanceCount;
        ResettersForTesting.register(() -> sIncognitoInstanceCountForTesting = null);
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
