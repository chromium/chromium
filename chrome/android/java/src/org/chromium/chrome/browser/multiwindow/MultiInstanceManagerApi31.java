// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.io.File;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

class MultiInstanceManagerApi31 extends MultiInstanceManager {
    public static final int INVALID_INSTANCE_ID = TabWindowManager.INVALID_WINDOW_INDEX;
    public static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected final int mMaxInstances;

    MultiInstanceManagerApi31(Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
        super(activity, tabModelOrchestratorSupplier, multiWindowModeStateDispatcher,
                activityLifecycleDispatcher, menuOrKeyboardActionController);
        mMaxInstances = TabWindowManagerSingleton.getInstance().getMaxSimultaneousSelectors();
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
            if (!stateFileExists(i) || getTaskFromMap(i) == INVALID_TASK_ID) return i;
        }
        return INVALID_INSTANCE_ID;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected boolean stateFileExists(int index) {
        // TODO: Define new entries in shared preferences for tab state rather than
        //       accessing disk file tab_state[0-5].
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            File stateFile = new File(TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                    TabbedModeTabPersistencePolicy.getStateFileName(index));
            return stateFile.exists();
        }
    }

    @Override
    public void updateTaskIdMap(int instanceId, int taskId) {
        SharedPreferencesManager.getInstance().writeInt(taskMapKey(instanceId), taskId);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static int getTaskFromMap(int index) {
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
}
