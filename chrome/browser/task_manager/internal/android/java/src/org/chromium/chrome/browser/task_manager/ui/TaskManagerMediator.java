// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.CPU;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.PROCESS_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import org.chromium.chrome.browser.task_manager.RefreshType;
import org.chromium.chrome.browser.task_manager.TaskManagerObserver;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.RowType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.NoSuchElementException;

/**
 * The class works as a mediator between the underlyning model (ModelList) and the task manager
 * backend accessed through TaskManagerServiceBridge.
 */
class TaskManagerMediator {
    private final int mRefreshTimeMillis;
    private final TaskManagerServiceBridge mBridge = new TaskManagerServiceBridge();
    private TaskManagerServiceBridge.ObserverHandle mObserverHandle;
    private final PropertyKey[] mColumnKeys;

    private final PropertyModel mHeader;
    // The list containing the properties representing tasks. Sorted by task id.
    // TODO(crbug.com/380154224): Enable sorting by other attributes.
    private final ModelList mTasks;

    /**
     * Constructs the mediator backed by the modelList.
     *
     * @param refreshTimeMillis How often the model should be refreshed.
     * @param header The model for the header. The model must accept COLUMNS as a key.
     * @param tasks The model for tasks.
     * @param columnKeys The columns to be updated. TASK_ID must not be set.
     */
    TaskManagerMediator(
            int refreshTimeMillis,
            PropertyModel header,
            ModelList tasks,
            PropertyKey... columnKeys) {
        mRefreshTimeMillis = refreshTimeMillis;
        mHeader = header;
        mTasks = tasks;
        mColumnKeys = columnKeys;

        mHeader.set(COLUMNS, columnKeys);
    }

    /** Start observing tasks to get the model updated. */
    void startObserving() {
        if (mObserverHandle == null) {
            TaskManagerObserver tmObserver = createTaskManagerObserver();
            // TODO(crbug.com/380154740): Update only the refreshType when columns are
            // added/removed.
            @RefreshType int refreshType = 0;
            for (PropertyKey columnKey : mColumnKeys) {
                refreshType |= getRequiredRefreshType(columnKey);
            }
            mObserverHandle = mBridge.addObserver(tmObserver, mRefreshTimeMillis, refreshType);
        }
    }

    /** Stop observing tasks. It should be called when the last user of the model is gone. */
    void stopObserving() {
        if (mObserverHandle != null) {
            mBridge.removeObserver(mObserverHandle);
            mObserverHandle = null;

            mTasks.clear();
        }
    }

    private @RefreshType int getRequiredRefreshType(PropertyKey columnKey) {
        if (columnKey == TASK_NAME) {
            return 0;
        } else if (columnKey == MEMORY_FOOTPRINT) {
            return RefreshType.MEMORY_FOOTPRINT;
        } else if (columnKey == CPU) {
            return RefreshType.CPU;
        } else if (columnKey == PROCESS_ID) {
            return 0;
        }
        throw new IllegalArgumentException();
    }

    private ListItem createTaskModel(long taskId) {
        PropertyKey[] keys = PropertyModel.concatKeys(mColumnKeys, new PropertyKey[] {TASK_ID});
        return new ListItem(
                RowType.TASK, new PropertyModel.Builder(keys).with(TASK_ID, taskId).build());
    }

    // TODO(crbug.com/380165957): Confirm pid and task name never change and stop
    // refreshing them.
    private void updateTaskModel(ListItem task, long taskId) {
        for (PropertyKey columnKey : mColumnKeys) {
            if (columnKey == TASK_NAME) {
                task.model.set(TASK_NAME, mBridge.getTitle(taskId));
            } else if (columnKey == MEMORY_FOOTPRINT) {
                task.model.set(MEMORY_FOOTPRINT, mBridge.getMemoryFootprintUsage(taskId));
            } else if (columnKey == CPU) {
                task.model.set(CPU, (float) mBridge.getPlatformIndependentCpuUsage(taskId));
            } else if (columnKey == PROCESS_ID) {
                task.model.set(PROCESS_ID, mBridge.getProcessId(taskId));
            } else {
                throw new IllegalArgumentException("column key " + columnKey + " not supported");
            }
        }
    }

    private TaskManagerObserver createTaskManagerObserver() {
        return new TaskManagerObserver() {
            @Override
            public void onTaskAdded(long taskId) {
                ListItem task = createTaskModel(taskId);
                updateTaskModel(task, taskId);

                int insertPos = 0;
                for (; insertPos < mTasks.size(); insertPos++) {
                    if (mTasks.get(insertPos).model.get(TASK_ID) > taskId) break;
                }
                mTasks.add(insertPos, task);
            }

            @Override
            public void onTaskToBeRemoved(long taskId) {
                mTasks.removeAt(getIndexForTaskId(taskId));
            }

            @Override
            public void onTasksRefreshed(long[] taskIds) {
                // TODO(crbug.com/380165957): Asynchronously get values if performance issues are
                // observed.
                // TODO(crbug.com/380165957): Confirm task ids are always sorted, and utilize this
                // to speed up the computation when the model is sorted by task id.
                for (long taskId : taskIds) {
                    ListItem task = mTasks.get(getIndexForTaskId(taskId));

                    updateTaskModel(task, taskId);
                }
            }

            @Override
            public void onTasksRefreshedWithBackgroundCalculations(long[] taskIds) {}

            @Override
            public void onTaskUnresponsive(long taskId) {}

            @Override
            public void onActiveTaskFetched(long taskId) {}
        };
    }

    private int getIndexForTaskId(long taskId) {
        for (int i = 0; i < mTasks.size(); i++) {
            if (mTasks.get(i).model.get(TASK_ID) == taskId) {
                return i;
            }
        }
        throw new NoSuchElementException("Task id " + taskId + " not found");
    }
}
