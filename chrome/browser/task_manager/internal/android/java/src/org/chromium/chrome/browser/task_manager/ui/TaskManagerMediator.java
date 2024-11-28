// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.CPU;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.IS_SELECTED;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.PROCESS_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SORT_DESCRIPTOR;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.task_manager.RefreshType;
import org.chromium.chrome.browser.task_manager.TaskManagerObserver;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.RowType;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SortDescriptor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Comparator;
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
    private boolean mHasSelectedTask;

    private @Nullable Callback<Boolean> mHasSelectedTaskChangedCallback;

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
        mHeader.set(SORT_DESCRIPTOR, null);
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
            mHasSelectedTask = false;
        }
    }

    /** Kill the currently selected tasks. */
    void killSelectedTasks() {
        for (ListItem task : mTasks) {
            if (task.model.get(IS_SELECTED)) {
                mBridge.killTask(task.model.get(TASK_ID));
            }
        }
    }

    /**
     * Toggle the selection of the task. If the task is selected, clear the selection of other
     * tasks.
     */
    void toggleSelection(PropertyModel taskModel) {
        boolean select = !taskModel.get(IS_SELECTED);
        taskModel.set(IS_SELECTED, select);
        if (select) {
            for (ListItem task : mTasks) {
                if (task.model != taskModel && task.model.get(IS_SELECTED)) {
                    task.model.set(IS_SELECTED, false);
                }
            }
        }
        checkAndNotifyIfTaskSelectionChanged();
    }

    /** Set a callback called when the existence of a selected task changes. */
    void onHasSelectedTaskChanged(Callback<Boolean> callback) {
        mHasSelectedTaskChangedCallback = callback;
    }

    /**
     * Cycles through the sort orderings for the key.
     *
     * <ul>
     *   <li>If the column is not sorted, sorts it in descending order.
     *   <li>If the column is sorted in descending order, sorts it in ascending order.
     *   <li>If the column is sorted in ascending order, removes the sorting.
     * </ul>
     */
    void cycleSortOrder(PropertyKey columnKey) {
        @Nullable SortDescriptor descriptor = mHeader.get(SORT_DESCRIPTOR);

        // TODO(crbug.com/381351182): change default sort order per column type.
        if (descriptor != null && descriptor.key == columnKey) {
            if (descriptor.ascending) {
                setSortDescriptor(null);
            } else {
                setSortDescriptor(new SortDescriptor(columnKey, true));
            }
        } else {
            setSortDescriptor(new SortDescriptor(columnKey, false));
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
        PropertyKey[] keys =
                PropertyModel.concatKeys(mColumnKeys, new PropertyKey[] {TASK_ID, IS_SELECTED});
        return new ListItem(
                RowType.TASK,
                new PropertyModel.Builder(keys)
                        .with(TASK_ID, taskId)
                        .with(IS_SELECTED, false)
                        .build());
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

                @Nullable SortDescriptor descriptor = mHeader.get(SORT_DESCRIPTOR);
                if (descriptor == null) {
                    mTasks.add(task);
                    return;
                }

                Comparator<ListItem> comparator = getTaskComparator(descriptor);

                int insertPos = 0;
                for (; insertPos < mTasks.size(); insertPos++) {
                    if (comparator.compare(task, mTasks.get(insertPos)) < 0) {
                        break;
                    }
                }
                mTasks.add(insertPos, task);
            }

            @Override
            public void onTaskToBeRemoved(long taskId) {
                mTasks.removeAt(getIndexForTaskId(taskId));
                checkAndNotifyIfTaskSelectionChanged();
            }

            @Override
            public void onTasksRefreshed(long[] taskIds) {
                // TODO(crbug.com/380165957): Asynchronously get values if performance issues are
                // observed.
                for (long taskId : taskIds) {
                    ListItem task = mTasks.get(getIndexForTaskId(taskId));

                    updateTaskModel(task, taskId);
                }

                SortDescriptor descriptor = mHeader.get(TaskManagerProperties.SORT_DESCRIPTOR);
                if (descriptor != null) {
                    sortTasks(descriptor);
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

    private void checkAndNotifyIfTaskSelectionChanged() {
        boolean hasSelectedTask = false;
        for (ListItem task : mTasks) {
            if (task.model.get(IS_SELECTED)) {
                hasSelectedTask = true;
                break;
            }
        }
        if (mHasSelectedTask != hasSelectedTask) {
            mHasSelectedTask = hasSelectedTask;
            if (mHasSelectedTaskChangedCallback != null) {
                mHasSelectedTaskChangedCallback.onResult(hasSelectedTask);
            }
        }
    }

    private void setSortDescriptor(@Nullable SortDescriptor descriptor) {
        mHeader.set(SORT_DESCRIPTOR, descriptor);

        if (descriptor != null) {
            sortTasks(descriptor);
        }
    }

    private void sortTasks(SortDescriptor descriptor) {
        Comparator<ListItem> comparator = getTaskComparator(descriptor);

        boolean isSorted = true;
        for (int i = 0; i < mTasks.size() - 1; i++) {
            if (comparator.compare(mTasks.get(i), mTasks.get(i + 1)) > 0) {
                isSorted = false;
                break;
            }
        }
        // Avoid unnecessary notification to the view that might cause flickering.
        if (isSorted) return;

        ArrayList<ListItem> tasks = new ArrayList<>();
        for (ListItem task : mTasks) tasks.add(task);
        tasks.sort(comparator);

        mTasks.set(tasks);
    }

    private static Comparator<ListItem> getTaskComparator(@NonNull SortDescriptor descriptor) {
        Comparator<ListItem> ascComparator =
                (a, b) -> {
                    if (descriptor.key == TASK_NAME) {
                        return a.model.get(TASK_NAME).compareTo(b.model.get(TASK_NAME));
                    } else if (descriptor.key == MEMORY_FOOTPRINT) {
                        return Long.compare(
                                a.model.get(MEMORY_FOOTPRINT), b.model.get(MEMORY_FOOTPRINT));
                    } else if (descriptor.key == CPU) {
                        return Float.compare(a.model.get(CPU), b.model.get(CPU));
                    } else if (descriptor.key == PROCESS_ID) {
                        return Long.compare(a.model.get(PROCESS_ID), b.model.get(PROCESS_ID));
                    } else {
                        throw new IllegalArgumentException(
                                "column key " + descriptor.key + " not supported");
                    }
                };
        return descriptor.ascending ? ascComparator : ascComparator.reversed();
    }
}
