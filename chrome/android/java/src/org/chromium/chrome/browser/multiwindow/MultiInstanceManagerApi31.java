// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceState.MultiInstanceStateObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.InstanceAllocationType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

class MultiInstanceManagerApi31 extends MultiInstanceManager implements ActivityStateListener {
    private static final String TAG = "MIMApi31";
    private static final String TAG_MULTI_INSTANCE = "MultiInstance";

    public static final int INVALID_INSTANCE_ID = MultiWindowUtils.INVALID_INSTANCE_ID;
    public static final int INVALID_TASK_ID = MultiWindowUtils.INVALID_TASK_ID;

    private static final String EMPTY_DATA = "";
    private static MultiInstanceState sState;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected final int mMaxInstances;

    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // Instance ID for the activity associated with this manager.
    private int mInstanceId = INVALID_INSTANCE_ID;

    private Tab mActiveTab;
    private TabObserver mActiveTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    if (!tab.isIncognito()) writeTitle(mInstanceId, tab);
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    if (!tab.isIncognito()) writeUrl(mInstanceId, tab);
                }
            };

    private final Supplier<DesktopWindowStateProvider> mDesktopWindowStateProviderSupplier;
    private final MultiInstanceStateObserver mOnMultiInstanceStateChanged;

    MultiInstanceManagerApi31(
            Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<DesktopWindowStateProvider> desktopWindowStateProviderSupplier) {
        super(
                activity,
                tabModelOrchestratorSupplier,
                multiWindowModeStateDispatcher,
                activityLifecycleDispatcher,
                menuOrKeyboardActionController);
        mMaxInstances = MultiWindowUtils.getMaxInstances();
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mDesktopWindowStateProviderSupplier = desktopWindowStateProviderSupplier;
        mOnMultiInstanceStateChanged = this::onMultiInstanceStateChanged;
    }

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == org.chromium.chrome.R.id.manage_all_windows_menu_id) {
            List<InstanceInfo> info = getInstanceInfo();
            InstanceSwitcherCoordinator.showDialog(
                    mActivity,
                    mModalDialogManagerSupplier.get(),
                    new LargeIconBridge(getProfile()),
                    (item) -> openInstance(item.instanceId, item.taskId),
                    (item) -> {
                        RecordUserAction.record("MobileMenuWindowManagerCloseInstance");
                        closeInstance(item.instanceId, item.taskId);
                        cleanupSyncedTabGroupsIfLastInstance();
                    },
                    () -> openNewWindow("Android.WindowManager.NewWindow"),
                    info.size() < MultiWindowUtils.getMaxInstances(),
                    info);

            if (AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProviderSupplier.get())) {
                RecordUserAction.record("MobileMenuWindowManager.InDesktopWindow");
            } else {
                RecordUserAction.record("MobileMenuWindowManager");
            }

            AppHeaderUtils.recordDesktopWindowModeStateEnumHistogram(
                    mDesktopWindowStateProviderSupplier.get(),
                    "Android.MultiInstance.WindowManager.DesktopWindowModeState");

            Tracker tracker = TrackerFactory.getTrackerForProfile(getProfile());
            assert tracker.isInitialized();
            tracker.notifyEvent(EventConstants.INSTANCE_SWITCHER_IPH_USED);
            return true;
        }
        return super.handleMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected void moveTabToOtherWindow(Tab tab) {
        TargetSelectorCoordinator.showDialog(
                mActivity,
                mModalDialogManagerSupplier.get(),
                new LargeIconBridge(getProfile()),
                (instanceInfo) -> {
                    moveTabAction(instanceInfo, tab, TabList.INVALID_TAB_INDEX);

                    // Close the source instance window, if needed.
                    closeChromeWindowIfEmpty(mInstanceId);
                },
                getInstanceInfo());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void moveTabAction(InstanceInfo info, Tab tab, int tabAtIndex) {
        Activity targetActivity = getActivityById(info.instanceId);
        if (targetActivity != null) {
            reparentTabToRunningActivity((ChromeTabbedActivity) targetActivity, tab, tabAtIndex);
        } else {
            moveAndReparentTabToNewWindow(
                    tab,
                    info.instanceId,
                    /* preferNew= */ false,
                    /* openAdjacently= */ true,
                    /* addTrustedIntentExtras= */ true);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void moveAndReparentTabToNewWindow(
            Tab tab,
            int instanceId,
            boolean preferNew,
            boolean openAdjacently,
            boolean addTrustedIntentExtras) {
        onMultiInstanceModeStarted();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity, instanceId, preferNew, openAdjacently, addTrustedIntentExtras);
        beginReparenting(
                tab,
                intent,
                mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions(),
                null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void reparentTabToRunningActivity(
            ChromeTabbedActivity targetActivity, Tab tab, int tabAtIndex) {
        assert targetActivity != null;
        Intent intent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        intent.setClassName(appContext, ChromeTabbedActivity.class.getName());
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, targetActivity.getClass());
        onMultiInstanceModeStarted();
        RecordUserAction.record("MobileMenuMoveToOtherWindow");

        if (tabAtIndex != TabList.INVALID_TAB_INDEX) {
            intent.putExtra(IntentHandler.EXTRA_TAB_INDEX, tabAtIndex);
        }
        setupIntentForReparenting(tab, intent, null);

        targetActivity.onNewIntent(intent);
        bringTaskForeground(targetActivity.getTaskId());
    }

    @Override
    protected void openNewWindow(String umaAction) {
        Intent intent = new Intent(mActivity, ChromeTabbedActivity.class);
        onMultiInstanceModeStarted();
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        IntentUtils.addTrustedIntentExtras(intent);
        if (mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
            Bundle bundle = mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions();
            mActivity.startActivity(intent, bundle);
        } else {
            mActivity.startActivity(intent);
        }
        Log.i(TAG_MULTI_INSTANCE, "Opening new window from action: " + umaAction);
        RecordUserAction.record(umaAction);
    }

    @Override
    public List<InstanceInfo> getInstanceInfo() {
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ false);
        List<InstanceInfo> result = new ArrayList<>();
        SparseBooleanArray visibleTasks = MultiWindowUtils.getVisibleTasks();
        int currentItemPos = -1;
        for (int i = 0; i < mMaxInstances; ++i) {
            if (!instanceEntryExists(i)) continue;
            @InstanceInfo.Type int type = InstanceInfo.Type.OTHER;
            Activity a = getActivityById(i);
            if (a != null) {
                // The task for the activity must match the one found in our mapping.
                assert getTaskFromMap(i) == a.getTaskId();
                if (a == mActivity) {
                    type = InstanceInfo.Type.CURRENT;
                    currentItemPos = result.size();
                } else if (isRunningInAdjacentWindow(visibleTasks, a)) {
                    type = InstanceInfo.Type.ADJACENT;
                }
            }

            int taskId = getTaskFromMap(i);
            result.add(
                    new InstanceInfo(
                            i,
                            taskId,
                            type,
                            readUrl(i),
                            readTitle(i),
                            readTabCount(i),
                            readIncognitoTabCount(i),
                            readIncognitoSelected(i)));
        }

        // Move the current instance always to the top of the list.
        assert currentItemPos != -1;
        if (currentItemPos != 0 && result.size() > 1) result.add(0, result.remove(currentItemPos));
        return result;
    }

    @Override
    public int getCurrentInstanceId() {
        List<InstanceInfo> allInstances = getInstanceInfo();
        if (allInstances == null || allInstances.isEmpty()) return INVALID_INSTANCE_ID;
        // Current instance is at top of list.
        return allInstances.get(0).instanceId;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected boolean isRunningInAdjacentWindow(
            SparseBooleanArray visibleTasks, Activity activity) {
        assert activity != mActivity;
        return visibleTasks.get(activity.getTaskId());
    }

    @Override
    public Pair<Integer, Integer> allocInstanceId(int windowId, int taskId, boolean preferNew) {
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ true);

        int instanceId = getInstanceByTask(taskId);

        // Explicitly specified window ID should be preferred. This comes from user selecting
        // a certain instance on UI when no task is present for it.
        // When out of range, ignore the ID and apply the normal allocation logic below.
        if (windowId >= 0 && windowId < mMaxInstances && instanceId == INVALID_INSTANCE_ID) {
            Log.i(TAG_MULTI_INSTANCE, "Existing Instance - selected Id allocated: " + windowId);
            return Pair.create(windowId, InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK);
        }

        // First, see if we have instance-task ID mapping. If we do, use the instance id. This
        // takes care of a task that had its activity destroyed and comes back to create a
        // new one. We pair them again.
        if (instanceId != INVALID_INSTANCE_ID) {
            Log.i(TAG_MULTI_INSTANCE, "Existing Instance - mapped Id allocated: " + instanceId);
            return Pair.create(instanceId, InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK);
        }

        // If asked to always create a fresh new instance, not from persistent state, do it here.
        if (preferNew) {
            for (int i = 0; i < mMaxInstances; ++i) {
                if (!instanceEntryExists(i)) {
                    logNewInstanceId(i);
                    return Pair.create(i, InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK);
                }
            }
            return Pair.create(
                    INVALID_INSTANCE_ID, InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE);
        }

        // Search for an unassigned ID. The index is available for the assignment if:
        // a) there is no associated task, or
        // b) the corresponding persistent state does not exist.
        // Prefer a over b. Pick the MRU instance if there is more than one. Type b returns 0
        // for |readLastAccessedTime|, so can be regarded as the least favored.
        int id = INVALID_INSTANCE_ID;
        boolean newInstanceIdAllocated = false;
        @InstanceAllocationType int allocationType = InstanceAllocationType.INVALID_INSTANCE;
        for (int i = 0; i < mMaxInstances; ++i) {
            int taskIdFromMap = getTaskFromMap(i);
            if (taskIdFromMap != INVALID_TASK_ID) {
                continue;
            }
            if (id == INVALID_INSTANCE_ID || readLastAccessedTime(i) > readLastAccessedTime(id)) {
                id = i;
                newInstanceIdAllocated = !instanceEntryExists(i);
                allocationType =
                        newInstanceIdAllocated
                                ? InstanceAllocationType.NEW_INSTANCE_NEW_TASK
                                : InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK;
            }
        }

        if (newInstanceIdAllocated) {
            logNewInstanceId(id);
        } else if (id != INVALID_INSTANCE_ID) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Existing Instance - persisted and unmapped Id allocated: " + id);
        }

        return Pair.create(id, allocationType);
    }

    private void logNewInstanceId(int i) {
        StringBuilder taskData = new StringBuilder();
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        for (AppTask task : activityManager.getAppTasks()) {
            String baseActivity = MultiWindowUtils.getActivityNameFromTask(task);
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            taskData.append(
                    "Task with id: "
                            + (info != null ? info.id : "NOT_SET")
                            + " has base activity: "
                            + baseActivity
                            + ".\n");
        }
        Log.i(
                TAG_MULTI_INSTANCE,
                "New Instance - unused Id allocated: "
                        + i
                        + ". Task data during instance allocation: "
                        + taskData);
    }

    @Override
    public void initialize(int instanceId, int taskId) {
        mInstanceId = instanceId;
        updateTaskMap(instanceId, taskId);
        installTabModelObserver();
        recordInstanceCountHistogram();
        recordActivityCountHistogram();
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        String launchActivityName = ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME;
        if (activityManager != null) {
            sState =
                    MultiInstanceState.maybeCreate(
                            activityManager::getAppTasks,
                            (activityName) ->
                                    TextUtils.equals(
                                                    activityName,
                                                    ChromeTabbedActivity.class.getName())
                                            || TextUtils.equals(activityName, launchActivityName));
            sState.addObserver(mOnMultiInstanceStateChanged);
        }
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onTabStateInitialized() {
        TabModelSelector selector = mTabModelOrchestratorSupplier.get().getTabModelSelector();
        writeTabCount(mInstanceId, selector);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void installTabModelObserver() {
        TabModelSelector selector = mTabModelOrchestratorSupplier.get().getTabModelSelector();
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        // We will check if |mActiveTab| is the same as the selected |tab| to avoid
                        // a superfluous update to an instance's stored active tab info that
                        // remains unchanged.
                        // The check on |lastId| is required to continue updating this info for an
                        // instance even when |mActiveTab| is the same as the selected |tab|, in
                        // the following scenario: If |mActiveTab| is the last tab in instance 1,
                        // and is moved to instance
                        // 2, instance 1 stores "empty" active tab information since it now
                        // contains no tabs.
                        // When |mActiveTab| is moved back to instance 1, |mActiveTab| is now the
                        // same as the selected |tab| in instance 1, however instance 1's active
                        // tab information will not be updated, unless we establish that this
                        // instance is currently holding "empty" info, reflected by the fact that
                        // it has an invalid last selected tab ID, so it's active tab info can
                        // then be updated.
                        if (mActiveTab == tab && lastId != Tab.INVALID_TAB_ID) return;
                        if (mActiveTab != null) mActiveTab.removeObserver(mActiveTabObserver);
                        mActiveTab = tab;
                        if (mActiveTab != null) {
                            mActiveTab.addObserver(mActiveTabObserver);
                            writeIncognitoSelected(mInstanceId, mActiveTab);
                            // When an incognito tab is focused, keep the normal active tab info.
                            Tab urlTab =
                                    mActiveTab.isIncognito()
                                            ? TabModelUtils.getCurrentTab(selector.getModel(false))
                                            : mActiveTab;
                            if (urlTab != null) {
                                writeUrl(mInstanceId, urlTab);
                                writeTitle(mInstanceId, urlTab);
                            } else {
                                writeUrl(mInstanceId, EMPTY_DATA);
                                writeTitle(mInstanceId, EMPTY_DATA);
                            }
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab, int type, int creationState, boolean markedForSelection) {
                        writeTabCount(mInstanceId, selector);
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab) {
                        // onFinishingTabClosure is called for both normal/incognito tabs, whereas
                        // tabClosureCommitted is called for normal tabs only.
                        writeTabCount(mInstanceId, selector);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        // Updates the tab count of the src activity a reparented tab gets detached
                        // from.
                        writeTabCount(mInstanceId, selector);
                    }
                };
    }

    static int getTaskFromMap(int index) {
        return ChromeSharedPreferences.getInstance().readInt(taskMapKey(index), INVALID_TASK_ID);
    }

    private static String taskMapKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(String.valueOf(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void updateTaskMap(int instanceId, int taskId) {
        ChromeSharedPreferences.getInstance().writeInt(taskMapKey(instanceId), taskId);
    }

    private void removeInvalidInstanceData(boolean cleanupApplicationStatus) {
        // Remove tasks that do not exist any more from the task map
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        Set<Integer> appTaskIds = getAllAppTaskIds(appTasks);
        Map<String, Integer> taskMap =
                ChromeSharedPreferences.getInstance()
                        .readIntsWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        List<String> tasksRemoved = new ArrayList<>();
        for (Map.Entry<String, Integer> entry : taskMap.entrySet()) {
            if (!appTaskIds.contains(entry.getValue())) {
                // TODO (crbug/327054706): Remove this check once we have verified that crash
                // reports have reduced.
                checkInvalidTaskNotInAllTasks(appTasks, entry.getValue());
                tasksRemoved.add(entry.getKey() + " - " + entry.getValue());
                ChromeSharedPreferences.getInstance().removeKey(entry.getKey());
                if (ChromeFeatureList.sMultiInstanceApplicationStatusCleanup.isEnabled()
                        && cleanupApplicationStatus) {
                    boolean foundTasks = ApplicationStatus.cleanupInvalidTask(entry.getValue());
                    if (foundTasks) {
                        if (BuildConfig.ENABLE_ASSERTS) {
                            String logMessage =
                                    "This is not a crash. Found tracked ApplicationStatus for Task "
                                            + " that no longer exists in #getAppTasks().";
                            ChromePureJavaExceptionReporter.reportJavaException(
                                    new Throwable(logMessage));
                        }
                    }
                }
            }
        }

        List<Integer> instancesRemoved = new ArrayList<>();
        // Remove persistent data for unrecoverable instances.
        for (int i = 0; i < mMaxInstances; ++i) {
            if (instanceEntryExists(i) && !MultiWindowUtils.isRestorableInstance(i)) {
                instancesRemoved.add(i);
                removeInstanceInfo(i);
            }
        }

        if (!tasksRemoved.isEmpty() || !instancesRemoved.isEmpty()) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Removed invalid instance data. Removed tasks-instance mappings: "
                            + tasksRemoved
                            + " and shared prefs for instances: "
                            + instancesRemoved);
        }
    }

    @VisibleForTesting
    protected static List<Activity> getAllRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    @VisibleForTesting
    protected Set<Integer> getAllAppTaskIds(List<AppTask> allTasks) {
        Set<Integer> results = new HashSet<>();
        for (AppTask task : allTasks) {
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info != null) results.add(info.taskId);
        }
        return results;
    }

    private void checkInvalidTaskNotInAllTasks(List<AppTask> allTasks, int removedTaskId) {
        if (!BuildConfig.ENABLE_ASSERTS || VERSION.SDK_INT < VERSION_CODES.Q) return;

        for (AppTask task : allTasks) {
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info != null && (info.id == removedTaskId || info.taskId == removedTaskId)) {
                String message =
                        "Removed instance data for Task still available in all app tasks. " + info;
                Log.i(TAG_MULTI_INSTANCE, message);
                if (info != null && info.isRunning) {
                    String crashMessage =
                            "This is not a crash. Removed instance data for running Task still"
                                    + " available in all app tasks. "
                                    + info;
                    ChromePureJavaExceptionReporter.reportJavaException(
                            new Throwable(crashMessage));
                }
                break;
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static Activity getActivityById(int id) {
        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        for (Activity activity : getAllRunningActivities()) {
            if (id == windowManager.getIndexForWindow(activity)) return activity;
        }
        return null;
    }

    private int getInstanceByTask(int taskId) {
        for (int i = 0; i < mMaxInstances; ++i) {
            if (taskId == getTaskFromMap(i)) return i;
        }
        return INVALID_INSTANCE_ID;
    }

    @Override
    public boolean isTabModelMergingEnabled() {
        return false;
    }

    private void recordActivityCountHistogram() {
        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumActivities",
                getRunningTabbedActivityCount(),
                mMaxInstances + 1);
    }

    static int getRunningTabbedActivityCount() {
        int numActivities = 0;
        List<Activity> activities = getAllRunningActivities();
        for (Activity activity : activities) {
            if (activity instanceof ChromeTabbedActivity) numActivities++;
        }
        return numActivities;
    }

    private void recordInstanceCountHistogram() {
        // Ensure we have instance info entry for the current one.
        writeLastAccessedTime(mInstanceId);

        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumInstances",
                MultiWindowUtils.getInstanceCount(),
                mMaxInstances + 1);
    }

    private static String incognitoSelectedKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED.createKey(
                String.valueOf(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void writeIncognitoSelected(int index, Tab tab) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(incognitoSelectedKey(index), tab.isIncognito());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean readIncognitoSelected(int index) {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(incognitoSelectedKey(index), false);
    }

    private static String urlKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static String readUrl(int index) {
        return ChromeSharedPreferences.getInstance().readString(urlKey(index), null);
    }

    static void writeUrl(int index, String url) {
        ChromeSharedPreferences.getInstance().writeString(urlKey(index), url);
    }

    private static void writeUrl(int index, Tab tab) {
        assert !tab.isIncognito();
        writeUrl(index, tab.getOriginalUrl().getSpec());
    }

    private static String titleKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(String.valueOf(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static String readTitle(int index) {
        return ChromeSharedPreferences.getInstance().readString(titleKey(index), null);
    }

    private static void writeTitle(int index, Tab tab) {
        assert !tab.isIncognito();
        writeTitle(index, tab.getTitle());
    }

    private static void writeTitle(int index, String title) {
        ChromeSharedPreferences.getInstance().writeString(titleKey(index), title);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static String tabCountKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(index));
    }

    static int readTabCount(int index) {
        return ChromeSharedPreferences.getInstance().readInt(tabCountKey(index));
    }

    private static String incognitoTabCountKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                String.valueOf(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static int readIncognitoTabCount(int index) {
        return ChromeSharedPreferences.getInstance().readInt(incognitoTabCountKey(index));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void writeTabCount(int index, TabModelSelector selector) {
        if (!selector.isTabStateInitialized()) return;
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int tabCount = selector.getModel(false).getCount();
        prefs.writeInt(tabCountKey(index), tabCount);
        prefs.writeInt(incognitoTabCountKey(index), selector.getModel(true).getCount());
        if (tabCount == 0) {
            writeUrl(index, EMPTY_DATA);
            writeTitle(index, EMPTY_DATA);
        }
    }

    static boolean instanceEntryExists(int index) {
        return readLastAccessedTime(index) != 0;
    }

    private static String lastAccessedTimeKey(int index) {
        return MultiWindowUtils.lastAccessedTimeKey(index);
    }

    private static long readLastAccessedTime(int index) {
        return MultiWindowUtils.readLastAccessedTime(index);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void writeLastAccessedTime(int index) {
        MultiWindowUtils.writeLastAccessedTime(index);
    }

    /**
     * @return The window IDs of the currently running ChromeTabbedActivity's. It is possible to
     *     have more number of saved instances than the number of currently running activities (for
     *     example, when an activity is killed from the Android app menu, its instance state still
     *     persists for use by Chrome).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    static SparseIntArray getWindowIdsOfRunningTabbedActivities() {
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        var windowIdsOfRunningTabbedActivities = new SparseIntArray();
        for (Activity activity : activities) {
            if (!(activity instanceof ChromeTabbedActivity)) continue;
            int windowId = TabWindowManagerSingleton.getInstance().getIndexForWindow(activity);
            windowIdsOfRunningTabbedActivities.put(windowId, windowId);
        }
        return windowIdsOfRunningTabbedActivities;
    }

    /**
     * Open or launch a given instance.
     *
     * @param instanceId ID of the instance to open.
     * @param taskId ID of the task the instance resides in.
     */
    @VisibleForTesting
    void openInstance(int instanceId, int taskId) {
        RecordUserAction.record("Android.WindowManager.SelectWindow");
        if (taskId != INVALID_TASK_ID) {
            // Bring the task to foreground if the activity is alive, this completes the opening
            // of the instance. Otherwise, create a new activity for the instance and kill the
            // existing task.
            // TODO: Consider killing the instance and start it again to be able to position it
            //       in the intended window.
            if (getActivityById(instanceId) != null) {
                bringTaskForeground(taskId);
                return;
            } else {
                var appTask = AndroidTaskUtils.getAppTaskFromId(mActivity, taskId);
                if (appTask != null) {
                    appTask.finishAndRemoveTask();
                }
            }
        }
        onMultiInstanceModeStarted();
        // TODO: Pass this flag from UI to control the window to open.
        boolean openAdjacently = true;
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        instanceId,
                        /* preferNew= */ false,
                        openAdjacently,
                        /* addTrustedIntentExtras= */ true);
        if (openAdjacently) {
            mActivity.startActivity(
                    intent, mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions());
        } else {
            mActivity.startActivity(intent);
        }
    }

    /**
     * Launch the given intent in an existing ChromeTabbedActivity instance.
     *
     * @param intent The intent to launch.
     * @param instanceId ID of the instance to launch the intent in.
     */
    static void launchIntentInInstance(Intent intent, int instanceId) {
        Activity activity = getActivityById(instanceId);
        if (!(activity instanceof ChromeTabbedActivity)) return;
        int taskId = activity.getTaskId();
        if (taskId != INVALID_TASK_ID) {
            // Launch the intent in the existing activity and bring the task to foreground if it is
            // alive.
            ((ChromeTabbedActivity) activity).onNewIntent(intent);
            var activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            activityManager.moveTaskToFront(taskId, 0);
        }
    }

    /**
     * Close a given task/activity instance.
     * @param instanceId ID of the activity instance.
     * @param taskId ID of the task including the activity.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void closeInstance(int instanceId, int taskId) {
        removeInstanceInfo(instanceId);
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance().getTabModelSelectorById(instanceId);
        if (selector != null) {
            // Close all tabs as the window is closing. This ensures the tabs are added to the
            // recent tabs page.
            //
            // TODO(crbug.com/40826734): This only works for windows with live activities. It is
            // non-trivial to add recent tab entries without an active {@link Tab} instance.
            var filterProvider = selector.getTabModelFilterProvider();
            TabClosureParams params =
                    TabClosureParams.closeAllTabs().uponExit(true).hideTabGroups(true).build();
            ((TabGroupModelFilter) filterProvider.getTabModelFilter(true)).closeTabs(params);
            ((TabGroupModelFilter) filterProvider.getTabModelFilter(false)).closeTabs(params);
        }
        mTabModelOrchestratorSupplier.get().cleanupInstance(instanceId);
        Activity activity = getActivityById(instanceId);
        if (activity != null) activity.finishAndRemoveTask();
    }

    @VisibleForTesting
    void bringTaskForeground(int taskId) {
        ActivityManager am = (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        am.moveTaskToFront(taskId, 0);
    }

    @VisibleForTesting
    void setupIntentForReparenting(Tab tab, Intent intent, Runnable finalizeCallback) {
        ReparentingTask.from(tab).setupIntent(mActivity, intent, finalizeCallback);
    }

    @VisibleForTesting
    void beginReparenting(
            Tab tab, Intent intent, Bundle startActivityOptions, Runnable finalizeCallback) {
        ReparentingTask.from(tab).begin(mActivity, intent, startActivityOptions, finalizeCallback);
    }

    private Profile getProfile() {
        return mTabModelOrchestratorSupplier
                .get()
                .getTabModelSelector()
                .getCurrentModel()
                .getProfile();
    }

    @Override
    public void onDestroy() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        // This handles a case where an instance is deleted within Chrome but not through
        // Window manager UI, and the task is removed by system. See https://crbug.com/1241719.
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ false);
        if (mInstanceId != INVALID_INSTANCE_ID) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
        if (sState != null) {
            if (getAllRunningActivities().isEmpty()) {
                sState.clear();
            } else {
                sState.removeObserver(mOnMultiInstanceStateChanged);
            }
        }

        super.onDestroy();
    }

    private static void removeInstanceInfo(int index) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(urlKey(index));
        prefs.removeKey(titleKey(index));
        prefs.removeKey(tabCountKey(index));
        prefs.removeKey(incognitoTabCountKey(index));
        prefs.removeKey(incognitoSelectedKey(index));
        prefs.removeKey(lastAccessedTimeKey(index));
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        writeLastAccessedTime(mInstanceId);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        if (newState != ActivityState.RESUMED && newState != ActivityState.STOPPED) return;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        // Check the max instance count in a day for every state update if needed.
        long timestamp = prefs.readLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, 0);
        int maxCount = prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, 0);
        long current = System.currentTimeMillis();

        if (current - timestamp > DateUtils.DAY_IN_MILLIS) {
            if (timestamp != 0) {
                RecordHistogram.recordExactLinearHistogram(
                        "Android.MultiInstance.MaxInstanceCount", maxCount, mMaxInstances + 1);
            }
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, current);
            // Reset the count to 0 to be ready to obtain the max count for the next 24-hour period.
            maxCount = 0;
        }
        int instanceCount = MultiWindowUtils.getInstanceCount();
        if (instanceCount > maxCount) {
            prefs.writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, instanceCount);
        }
    }

    private void onMultiInstanceStateChanged(boolean inMultiInstanceMode) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        long startTime = prefs.readLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME);
        long current = System.currentTimeMillis();

        // This method in invoked for every ChromeActivity instance. Logging metrics for the first
        // ChromeActivity is enough. The pref |MULTI_INSTANCE_START_TIME| is set to non-zero once
        // Android.MultiInstance.Enter is logged, and reset to zero after
        // Android.MultiInstance.Exit to avoid duplicated logging.
        if (startTime == 0 && inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Enter");
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, current);
        } else if (startTime != 0 && !inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Exit");
            RecordHistogram.recordLongTimesHistogram(
                    "Android.MultiInstance.TotalDuration", current - startTime);
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, 0);
        }
    }

    /**
     * Open a new instance of the ChromeTabbedActivity window and move the specified tab from
     * existing instance to the new one.
     *
     * @param tab Tab that is to be moved to a new Chrome instance.
     */
    @Override
    public void moveTabToNewWindow(Tab tab) {
        // Check if the new Chrome instance can be opened.
        if (MultiWindowUtils.getInstanceCount() < mMaxInstances) {
            moveAndReparentTabToNewWindow(
                    tab,
                    INVALID_INSTANCE_ID,
                    /* preferNew= */ true,
                    /* openAdjacently= */ false,
                    /* addTrustedIntentExtras= */ true);
        } else {
            // Just try to launch a Chrome window to inform user that maximum number of instances
            // limit is exceeded. This will pop up a toast message and the tab will not be removed
            // from the exiting window.
            openNewWindow("Android.WindowManager.NewWindow");
        }
    }

    /**
     * Move the specified tab to the current instance of the ChromeTabbedActivity window.
     *
     * @param activity Activity of the Chrome Window in which the tab is to be moved.
     * @param tab Tab that is to be moved to the current instance.
     * @param atIndex Tab position index in the destination window instance.
     */
    @Override
    public void moveTabToWindow(Activity activity, Tab tab, int atIndex) {
        // Get the current instance and move tab there.
        InstanceInfo info = getInstanceInfoFor(activity);
        if (info != null) {
            moveTabAction(info, tab, atIndex);
        } else {
            Log.w(TAG, "DnD: InstanceInfo of Chrome Window not found.");
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    InstanceInfo getInstanceInfoFor(Activity activity) {
        // Loop thru all instances to determine if the destination activity is present.
        int destinationWindowTaskId = INVALID_TASK_ID;
        for (int i = 0; i < mMaxInstances; ++i) {
            if (!instanceEntryExists(i)) continue;
            Activity activityById = getActivityById(i);
            if (activityById != null) {
                // The task for the activity must match the one found in our mapping.
                assert getTaskFromMap(i) == activityById.getTaskId();
                if (activityById == activity) {
                    destinationWindowTaskId = activityById.getTaskId();
                    break;
                }
            }
        }
        if (destinationWindowTaskId == INVALID_TASK_ID) return null;

        List<InstanceInfo> allInstances = getInstanceInfo();
        for (InstanceInfo instanceInfo : allInstances) {
            if (instanceInfo.taskId == destinationWindowTaskId) {
                return instanceInfo;
            }
        }
        return null;
    }

    /**
     * Determine if a Chrome instance can be closed based on the environment.
     *
     * @param instanceId Instance Id of the Chrome window that needs to be closed.
     */
    private boolean canCloseChromeWindow(int instanceId) {
        // Close the source instance window after tab reparenting if permitted by the feature flag
        // or if the app is in a desktop window, and the source instance is known.
        if (instanceId == INVALID_INSTANCE_ID) return false;

        return TabUiFeatureUtilities.isTabDragAsWindowEnabled()
                || AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProviderSupplier.get());
    }

    /**
     * Close a Chrome window instance only if it contains no open tabs including incognito ones.
     *
     * @param instanceId Instance id of the Chrome window that needs to be closed.
     * @return {@code true} if the window was closed, {@code false} otherwise.
     */
    @Override
    public boolean closeChromeWindowIfEmpty(int instanceId) {
        if (canCloseChromeWindow(instanceId)) {
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance().getTabModelSelectorById(instanceId);
            // Determine if the drag source Chrome instance window has any tabs including incognito
            // ones left so as to close if it is empty.
            if (selector.getTotalTabCount() == 0) {
                Log.i(TAG, "Closing empty Chrome instance as no tabs exist.");
                closeInstance(instanceId, INVALID_TASK_ID);
                return true;
            }
        }
        return false;
    }

    /**
     * This method makes a call out to sync to audit all of the tab groups if there is only one
     * remaining active Chrome instance. This is a workaround to the fact that closing an instance
     * that does not have an active {@link TabModelSelector} will never notify sync that the tabs it
     * contained were closed and as such sync will continue to think some inactive instance contains
     * the tab groups that aren't available in the current activity. If we get down to a single
     * instance of Chrome we know any data for tab groups not found in the current activity's {@link
     * TabModelSelector} must be closed and we can remove the sync mapping.
     */
    @VisibleForTesting
    void cleanupSyncedTabGroupsIfLastInstance() {
        List<InstanceInfo> info = getInstanceInfo();
        if (info.size() != 1) return;

        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance()
                        .getTabModelSelectorById(info.get(0).instanceId);
        assert selector != null;

        TabGroupModelFilter filter =
                (TabGroupModelFilter) selector.getTabModelFilterProvider().getTabModelFilter(false);

        Profile profile = filter.getTabModel().getProfile();
        if (!TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return;

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        TabGroupSyncUtils.unmapLocalIdsNotInTabGroupModelFilter(tabGroupSyncService, filter);
    }
}
