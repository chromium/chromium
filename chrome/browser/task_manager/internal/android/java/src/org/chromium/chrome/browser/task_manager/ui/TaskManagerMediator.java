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

import java.util.ArrayList;
import java.util.NoSuchElementException;

/**
 * The class works as a mediator between the underlyning model (ModelList) and the task manager
 * backend accessed through TaskManagerServiceBridge.
 */
class TaskManagerMediator {
    private static final int HEADER_OFFSET = 1;

    private final int mRefreshTimeMillis;
    private final TaskManagerServiceBridge mBridge = new TaskManagerServiceBridge();
    private TaskManagerServiceBridge.ObserverHandle mObserverHandle;
    private final ArrayList<PropertyKey> mColumnKeys = new ArrayList<>();

    // The list containing the properties representing tasks. Sorted by task id. The first item is
    // the header and the rest are tasks.
    // TODO(crbug.com/380154224): Enable sorting by other attributes.
    private final ModelList mModelList;

    /**
     * Constructs the mediator backed by the modelList.
     *
     * @param refreshTimeMillis How often the model should be refreshed.
     * @param modelList The backing model.
     * @param columnKeys The properties to be updated. TASK_ID must be set even if it's not used in
     *     UI.
     */
    TaskManagerMediator(int refreshTimeMillis, ModelList modelList, PropertyKey... columnKeys) {
        mRefreshTimeMillis = refreshTimeMillis;
        mModelList = modelList;
        for (PropertyKey columnKey : columnKeys) mColumnKeys.add(columnKey);

        ListItem header =
                new ListItem(
                        RowType.HEADER,
                        new PropertyModel.Builder(COLUMNS).with(COLUMNS, columnKeys).build());
        mModelList.add(header);
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

            mModelList.removeRange(HEADER_OFFSET, mModelList.size() - HEADER_OFFSET);
        }
    }

    private @RefreshType int getRequiredRefreshType(PropertyKey columnKey) {
        if (columnKey == TASK_ID) {
            return 0;
        } else if (columnKey == TASK_NAME) {
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

    private void updateProperty(ListItem item, long taskId, PropertyKey columnKey) {
        if (columnKey == TASK_ID) {
            item.model.set(TASK_ID, taskId);
        } else if (columnKey == TASK_NAME) {
            item.model.set(TASK_NAME, mBridge.getTitle(taskId));
        } else if (columnKey == MEMORY_FOOTPRINT) {
            item.model.set(MEMORY_FOOTPRINT, mBridge.getMemoryFootprintUsage(taskId));
        } else if (columnKey == CPU) {
            item.model.set(CPU, (float) mBridge.getPlatformIndependentCpuUsage(taskId));
        } else if (columnKey == PROCESS_ID) {
            item.model.set(PROCESS_ID, mBridge.getProcessId(taskId));
        } else {
            throw new IllegalArgumentException();
        }
    }

    private TaskManagerObserver createTaskManagerObserver() {
        return new TaskManagerObserver() {
            @Override
            public void onTaskAdded(long taskId) {
                ListItem taskItem = new ListItem(RowType.TASK, new PropertyModel(mColumnKeys));
                for (PropertyKey columnKey : mColumnKeys) {
                    updateProperty(taskItem, taskId, columnKey);
                }

                int insertPos = HEADER_OFFSET;
                for (; insertPos < mModelList.size(); insertPos++) {
                    if (mModelList.get(insertPos).model.get(TASK_ID) > taskId) break;
                }
                mModelList.add(insertPos, taskItem);
            }

            @Override
            public void onTaskToBeRemoved(long taskId) {
                mModelList.removeAt(getIndexForTaskId(taskId));
            }

            @Override
            public void onTasksRefreshed(long[] taskIds) {
                // TODO(crbug.com/380165957): Asynchronously get values if performance issues are
                // observed.
                // TODO(crbug.com/380165957): Confirm task ids are always sorted, and utilize this
                // to speed up the computation when the model is sorted by task id.
                for (long taskId : taskIds) {
                    ListItem taskItem = mModelList.get(getIndexForTaskId(taskId));

                    // TODO(crbug.com/380165957): Confirm pid and task name never change and stop
                    // refreshing them.
                    for (PropertyKey columnKey : mColumnKeys) {
                        if (columnKey == TASK_ID) {
                            continue; // already populated in onTaskAdded
                        }
                        updateProperty(taskItem, taskId, columnKey);
                    }
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
        for (int i = HEADER_OFFSET; i < mModelList.size(); i++) {
            if (mModelList.get(i).model.get(TASK_ID) == taskId) {
                return i;
            }
        }
        throw new NoSuchElementException("Task id " + taskId + " not found");
    }
}
