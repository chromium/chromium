// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

class MultiInstanceManagerApi31 extends MultiInstanceManager {
    public static final int INVALID_INSTANCE_ID = MultiWindowUtils.INVALID_INSTANCE_ID;
    public static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected final int mMaxInstances;

    // Instance ID for the activity associated with this manager.
    private int mInstanceId;

    private TabModelSelectorTabModelObserver mTabModelObserver;
    private Tab mActiveTab;
    private TabObserver mActiveTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab tab) {
            writeTitle(mInstanceId, tab.getTitle());
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            writeUrl(mInstanceId, tab.getOriginalUrl().getSpec());
        }
    };

    MultiInstanceManagerApi31(Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
        super(activity, tabModelOrchestratorSupplier, multiWindowModeStateDispatcher,
                activityLifecycleDispatcher, menuOrKeyboardActionController);
        mMaxInstances = MultiWindowUtils.getMaxInstances();
    }

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == org.chromium.chrome.R.id.manage_all_windows_menu_id) {
            InstanceSwitcherCoordinator.showDialog(
                    mActivity, this::openInstance, this::closeInstance, getInstanceInfo());
            return true;
        }
        return super.handleMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected void moveTabToOtherWindow(Tab tab) {
        // TODO: Implement the target instance selection UI. Make it possible to move tabs
        //       to uninstantiated activity too.
    }

    @Override
    protected void openNewWindow() {
        // TODO: Check if we already reached the maximum # of instances.
        Intent intent = new Intent(mActivity, ChromeTabbedActivity.class);
        onMultiInstanceModeStarted();
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        if (mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
            Bundle bundle = mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions();
            mActivity.startActivity(intent, bundle);
        } else {
            mActivity.startActivity(intent);
        }
        RecordUserAction.record("MobileMenuNewWindow");
    }

    @Override
    public List<InstanceInfo> getInstanceInfo() {
        removeInvalidEntriesFromTaskMap();
        List<InstanceInfo> result = new ArrayList<>();
        for (int i = 0; i < mMaxInstances; ++i) {
            String url = readUrl(i);
            if (url == null) continue;
            @InstanceInfo.Type
            int type = InstanceInfo.Type.OTHER;
            Activity a = getActivityById(i);
            if (a != null) {
                // The task for the activity must match the one found in our mapping.
                assert getTaskFromMap(i) == a.getTaskId();
                if (a == mActivity) {
                    type = InstanceInfo.Type.CURRENT;
                } else if (isRunningInAdjacentWindow(a)) {
                    type = InstanceInfo.Type.ADJACENT;
                }
            }

            int taskId = getTaskFromMap(i);
            result.add(new InstanceInfo(i, taskId, type, url, readTitle(i), readTabCount(i)));
        }
        return result;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected boolean isRunningInAdjacentWindow(Activity activity) {
        assert activity != mActivity;
        // TODO: Do more rigorous check to detect the adjacent instance.
        return ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED;
    }

    @Override
    public int allocInstanceId(int windowId, int taskId) {
        removeInvalidEntriesFromTaskMap();

        // Explicitly specific window ID should be preferred. This comes from user selecting
        // a certain instance on UI. This method would never be called if there were an instance
        // already mapped to the task. Check it with an assert.
        if (windowId != INVALID_INSTANCE_ID) {
            assert getInstanceByTask(taskId) == INVALID_INSTANCE_ID;
            return windowId;
        }

        // First, see if we have instance-task ID mapping. If we do, use the instance id. This
        // takes care of a task that had its activity destroyed and comes back to create a
        // new one. We pair them again.
        int instanceId = getInstanceByTask(taskId);
        if (instanceId != INVALID_INSTANCE_ID) return instanceId;

        for (int i = 0; i < mMaxInstances; ++i) {
            // The index is available for the assignment if:
            // 1) the corresponding state does not exist, or
            // 2) there is no associated task.
            // TODO: Prefer 2 to 1, and consider returning the most recently used one.
            if (readUrl(i) == null || getTaskFromMap(i) == INVALID_TASK_ID) return i;
        }
        return INVALID_INSTANCE_ID;
    }

    @Override
    public void initialize(int instanceId, int taskId) {
        mInstanceId = instanceId;
        SharedPreferencesManager.getInstance().writeInt(taskMapKey(instanceId), taskId);
        installTabModelObserver();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void installTabModelObserver() {
        TabModelSelector selector = mTabModelOrchestratorSupplier.get().getTabModelSelector();
        mTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (mActiveTab == tab) return;
                if (mActiveTab != null) mActiveTab.removeObserver(mActiveTabObserver);
                mActiveTab = tab;
                if (mActiveTab != null) {
                    mActiveTab.addObserver(mActiveTabObserver);
                    // TODO: Store incognito-related info.
                    writeUrl(mInstanceId, mActiveTab.getOriginalUrl().getSpec());
                    writeTitle(mInstanceId, mActiveTab.getTitle());
                }
            }

            @Override
            public void didAddTab(Tab tab, int type, int creationState) {
                writeTabCount(mInstanceId, selector.getTotalTabCount());
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                writeTabCount(mInstanceId, selector.getTotalTabCount());
            }
        };
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static int getTaskFromMap(int index) {
        return SharedPreferencesManager.getInstance().readInt(taskMapKey(index), INVALID_TASK_ID);
    }

    private static String taskMapKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(String.valueOf(index));
    }

    private void removeInvalidEntriesFromTaskMap() {
        // Remove tasks that do not exist any more.
        Set<Integer> validTasks = getAllChromeTasks();
        Map<String, Integer> taskMap = SharedPreferencesManager.getInstance().readIntsWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        for (Map.Entry<String, Integer> entry : taskMap.entrySet()) {
            if (!validTasks.contains(entry.getValue())) {
                SharedPreferencesManager.getInstance().removeKey(entry.getKey());
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected List<Activity> getAllRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected Set<Integer> getAllChromeTasks() {
        Set<Integer> results = new HashSet<>();
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        for (AppTask task : activityManager.getAppTasks()) {
            String baseActivity = MultiWindowUtils.getActivityNameFromTask(task);
            if (!TextUtils.equals(baseActivity, ChromeTabbedActivity.class.getName())) continue;
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info != null) results.add(info.id);
        }
        return results;
    }

    private Activity getActivityById(int id) {
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

    private static String urlKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(index));
    }

    private static String readUrl(int index) {
        return SharedPreferencesManager.getInstance().readString(urlKey(index), null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static void writeUrl(int index, String url) {
        SharedPreferencesManager.getInstance().writeString(urlKey(index), url);
    }

    private static String titleKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(String.valueOf(index));
    }

    private static String readTitle(int index) {
        return SharedPreferencesManager.getInstance().readString(titleKey(index), null);
    }

    private static void writeTitle(int index, String title) {
        SharedPreferencesManager.getInstance().writeString(titleKey(index), title);
    }

    private static String tabCountKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(index));
    }

    private static int readTabCount(int index) {
        return SharedPreferencesManager.getInstance().readInt(tabCountKey(index));
    }

    private static void writeTabCount(int index, int tabCount) {
        SharedPreferencesManager.getInstance().writeInt(tabCountKey(index), tabCount);
    }

    /**
     * Open or launch a given instance.
     * @param instanceId ID of the instance to open.
     * @param taskId ID of the task the instance resides in.
     * @param openAdjacently Whether the instance should be launched in the adjacent window.
     */
    private void openInstance(int instanceId, int taskId, boolean openAdjacently) {
        if (taskId != INVALID_TASK_ID) {
            // Just bring the task foreground if it is alive. This either completes the opening
            // of the instance or leads to creating a new activity.
            // TODO: Consider killing the instance and start it again to be able to position it
            //       in the intended window.
            bringTaskForeground(taskId);
            return;
        }
        onMultiInstanceModeStarted();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(mActivity, instanceId, openAdjacently);
        if (openAdjacently) {
            mActivity.startActivity(
                    intent, mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions());
        } else {
            mActivity.startActivity(intent);
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
        // TODO: Delete persistent instance/tab state files too.
        Activity activity = getActivityById(instanceId);
        if (activity != null) ApiCompatibilityUtils.finishAndRemoveTask(activity);
    }

    private void bringTaskForeground(int taskId) {
        ActivityManager am = (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        am.moveTaskToFront(taskId, 0);
    }

    @Override
    public void onDestroy() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        super.onDestroy();
    }

    private static void removeInstanceInfo(int index) {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.removeKey(urlKey(index));
        prefs.removeKey(titleKey(index));
        prefs.removeKey(tabCountKey(index));
    }
}
