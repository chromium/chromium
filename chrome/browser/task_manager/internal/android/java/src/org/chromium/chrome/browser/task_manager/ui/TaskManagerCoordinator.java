// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.graphics.Typeface;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SortDescriptor;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Binds the model and the view of task manager. */
class TaskManagerCoordinator implements OnCreateContextMenuListener {
    private final PropertyModel mHeaderModel;

    private final TaskManagerMediator mMediator;

    private final SimpleRecyclerViewAdapter mAdapter;
    private final List<PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>>
            mModelChangeProcessors = new ArrayList<>();

    /** Sets up the UI in the activity, binding the activity and the model. */
    TaskManagerCoordinator(
            View taskManagerView,
            PropertyModel headerModel,
            ModelList tasksModel,
            TaskManagerMediator mediator) {
        mHeaderModel = headerModel;
        mMediator = mediator;

        LinearLayout headerView = taskManagerView.findViewById(R.id.header_linear_layout);
        mModelChangeProcessors.add(
                PropertyModelChangeProcessor.create(
                        headerModel,
                        headerView,
                        (model, view, key) -> {
                            for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
                                view.findViewById(getTaskItemViewId(columnKey))
                                        .setOnClickListener(
                                                (unused) -> mMediator.cycleSortOrder(columnKey));
                            }
                            bindHeader(model, view, key);
                        }));

        RecyclerView recyclerView = taskManagerView.findViewById(R.id.tasks_view);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(
                        recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));

        mAdapter = new SimpleRecyclerViewAdapter(tasksModel);
        recyclerView.setAdapter(mAdapter);

        mAdapter.registerType(
                TaskManagerProperties.RowType.TASK,
                (parent) ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.task_item, parent, false),
                (model, view, key) -> {
                    mModelChangeProcessors.add(
                            PropertyModelChangeProcessor.create(
                                    headerModel,
                                    view,
                                    TaskManagerCoordinator::bindHeaderModelAndTaskView));

                    view.setOnClickListener((unused) -> mMediator.toggleSelection(model));
                    bindTask(model, view, key);
                });

        ButtonCompat killButton = taskManagerView.findViewById(R.id.kill_button);
        killButton.setText(R.string.task_manager_kill);
        killButton.setEnabled(false);
        killButton.setOnClickListener((view) -> mMediator.killSelectedTasks());
        mMediator.onHasKillableSelectedTaskChanged(
                (hasSelectedTask) -> killButton.setEnabled(hasSelectedTask));

        taskManagerView.setOnCreateContextMenuListener(this);

        mMediator.startObserving();
    }

    /** Revert the bindings made in the constructor. */
    void destroy() {
        mMediator.stopObserving();

        mModelChangeProcessors.forEach(PropertyModelChangeProcessor::destroy);
        mAdapter.destroy();
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        onCreateContextMenuImpl(menu);
    }

    @VisibleForTesting
    void onCreateContextMenuImpl(Menu menu) {
        Set<PropertyKey> selectedColumns = Set.of(mHeaderModel.get(TaskManagerProperties.COLUMNS));

        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            MenuItem item = menu.add(getColumnTextResourceId(columnKey));
            item.setCheckable(true);
            item.setChecked(selectedColumns.contains(columnKey));

            item.setOnMenuItemClickListener(
                    (unused) -> {
                        if (mMediator.toggleColumnFiltering(columnKey)) {
                            // Handle the visual update as it is being dismissed.
                            item.setChecked(!item.isChecked());
                        }
                        return true;
                    });
        }
    }

    private static void bindHeader(PropertyModel model, View view, PropertyKey unused) {
        @Nullable SortDescriptor descriptor = model.get(TaskManagerProperties.SORT_DESCRIPTOR);
        Set<PropertyKey> selectedKeys = Set.of(model.get(TaskManagerProperties.COLUMNS));

        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            TextView textView = view.findViewById(getTaskItemViewId(columnKey));
            if (!selectedKeys.contains(columnKey)) {
                textView.setVisibility(View.GONE);
                continue;
            }
            textView.setVisibility(View.VISIBLE);

            textView.setText(getColumnTextResourceId(columnKey));

            // TOOD(crbug.com/380158700): Descriptive message for a11y.
            if (descriptor != null && descriptor.key == columnKey) {
                if (descriptor.ascending) {
                    textView.append(" ▲");
                } else {
                    textView.append(" ▼");
                }
            }

            textView.setTypeface(null, Typeface.BOLD);
        }
    }

    private static void bindTask(PropertyModel model, View view, PropertyKey key) {
        if (key == TaskManagerProperties.IS_SELECTED) {
            if (model.get(TaskManagerProperties.IS_SELECTED)) {
                view.setBackgroundColor(
                        SemanticColorUtils.getColorPrimaryContainer(view.getContext()));
            } else {
                view.setBackgroundColor(0);
            }
            return;
        }
        if (!List.of(TaskManagerProperties.ALL_COLUMN_KEYS).contains(key)) {
            return;
        }

        TextView textView = view.findViewById(getTaskItemViewId(key));

        if (key == TaskManagerProperties.TASK_NAME) {
            textView.setText(model.get(TaskManagerProperties.TASK_NAME));
        } else if (key == TaskManagerProperties.MEMORY_FOOTPRINT) {
            textView.setText(
                    PropertyStringifier.getMemoryUsageText(
                            view.getContext(), model.get(TaskManagerProperties.MEMORY_FOOTPRINT)));
        } else if (key == TaskManagerProperties.CPU) {
            textView.setText(
                    PropertyStringifier.getCpuUsageText(
                            view.getContext(), model.get(TaskManagerProperties.CPU)));
        } else if (key == TaskManagerProperties.NETWORK_USAGE) {
            textView.setText(
                    PropertyStringifier.getNetworkUsageText(
                            view.getContext(), model.get(TaskManagerProperties.NETWORK_USAGE)));
        } else if (key == TaskManagerProperties.PROCESS_ID) {
            textView.setText(String.valueOf(model.get(TaskManagerProperties.PROCESS_ID)));
        } else if (key == TaskManagerProperties.GPU_MEMORY) {
            textView.setText(
                    PropertyStringifier.getMemoryUsageText(
                            view.getContext(), model.get(TaskManagerProperties.GPU_MEMORY)));
        } else {
            throw new IllegalArgumentException("column key " + key + " not supported");
        }
    }

    private static void bindHeaderModelAndTaskView(
            PropertyModel model, View view, PropertyKey key) {
        if (key != TaskManagerProperties.COLUMNS) return;

        Set<PropertyKey> selectedKeys = Set.of(model.get(TaskManagerProperties.COLUMNS));
        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            View textView = view.findViewById(getTaskItemViewId(columnKey));
            if (selectedKeys.contains(columnKey)) {
                textView.setVisibility(View.VISIBLE);
            } else {
                textView.setVisibility(View.GONE);
            }
        }
    }

    /** Converts the given header column key to the resource id of the text for the column. */
    static @StringRes int getColumnTextResourceId(PropertyKey columnKey) {
        if (columnKey == TaskManagerProperties.TASK_NAME) {
            return R.string.task_manager_task_column;
        } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
            return R.string.task_manager_mem_footprint_column;
        } else if (columnKey == TaskManagerProperties.CPU) {
            return R.string.task_manager_cpu_column;
        } else if (columnKey == TaskManagerProperties.NETWORK_USAGE) {
            return R.string.task_manager_net_column;
        } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
            return R.string.task_manager_process_id_column;
        } else if (columnKey == TaskManagerProperties.GPU_MEMORY) {
            return R.string.task_manager_video_memory_column;
        } else {
            throw new IllegalArgumentException("column key " + columnKey + " not supported");
        }
    }

    /**
     * Converts the given header column key to the resource id of the corresponding view in the task
     * item view.
     */
    static @IdRes int getTaskItemViewId(PropertyKey columnKey) {
        if (columnKey == TaskManagerProperties.TASK_NAME) {
            return R.id.task_name;
        } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
            return R.id.memory_footprint;
        } else if (columnKey == TaskManagerProperties.CPU) {
            return R.id.cpu;
        } else if (columnKey == TaskManagerProperties.NETWORK_USAGE) {
            return R.id.network_usage;
        } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
            return R.id.process_id;
        } else if (columnKey == TaskManagerProperties.GPU_MEMORY) {
            return R.id.gpu_memory_id;
        } else {
            throw new IllegalArgumentException("column key " + columnKey + " not supported");
        }
    }
}
