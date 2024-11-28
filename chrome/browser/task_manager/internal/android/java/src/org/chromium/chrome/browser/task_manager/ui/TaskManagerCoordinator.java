// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.graphics.Typeface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
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

/** Binds the model and the view of task manager. */
class TaskManagerCoordinator {
    private final TaskManagerMediator mMediator;

    private final SimpleRecyclerViewAdapter mAdapter;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>
            mHeaderChangeProcessor;

    /** Sets up the UI in the activity, binding the activity and the model. */
    TaskManagerCoordinator(
            View taskManagerView,
            PropertyModel headerModel,
            ModelList tasksModel,
            TaskManagerMediator mediator) {
        mMediator = mediator;

        LinearLayout headerView = taskManagerView.findViewById(R.id.header_linear_layout);
        mHeaderChangeProcessor =
                PropertyModelChangeProcessor.create(
                        headerModel,
                        headerView,
                        (model, view, key) -> {
                            view.findViewById(R.id.task_name)
                                    .setOnClickListener(
                                            (unused) ->
                                                    mMediator.cycleSortOrder(
                                                            TaskManagerProperties.TASK_NAME));
                            view.findViewById(R.id.memory_footprint)
                                    .setOnClickListener(
                                            (unused) ->
                                                    mMediator.cycleSortOrder(
                                                            TaskManagerProperties
                                                                    .MEMORY_FOOTPRINT));
                            view.findViewById(R.id.cpu)
                                    .setOnClickListener(
                                            (unused) ->
                                                    mMediator.cycleSortOrder(
                                                            TaskManagerProperties.CPU));
                            view.findViewById(R.id.process_id)
                                    .setOnClickListener(
                                            (unused) ->
                                                    mMediator.cycleSortOrder(
                                                            TaskManagerProperties.PROCESS_ID));

                            bindHeader(model, view, key);
                        });

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
                    view.setOnClickListener((unused) -> mMediator.toggleSelection(model));
                    bindTask(model, view, key);
                });

        ButtonCompat killButton = taskManagerView.findViewById(R.id.kill_button);
        killButton.setText(R.string.task_manager_kill);
        killButton.setEnabled(false);
        killButton.setOnClickListener((view) -> mMediator.killSelectedTasks());

        mMediator.onHasSelectedTaskChanged(
                (hasSelectedTask) -> killButton.setEnabled(hasSelectedTask));
        mMediator.startObserving();
    }

    /** Revert the bindings made in the constructor. */
    void destroy() {
        mMediator.stopObserving();

        mAdapter.destroy();
        mHeaderChangeProcessor.destroy();
    }

    private static void bindHeader(PropertyModel model, View view, PropertyKey unused) {
        @Nullable SortDescriptor descriptor = model.get(TaskManagerProperties.SORT_DESCRIPTOR);

        for (PropertyKey columnKey : model.get(TaskManagerProperties.COLUMNS)) {
            TextView textView;
            if (columnKey == TaskManagerProperties.TASK_NAME) {
                textView = view.findViewById(R.id.task_name);
                textView.setText(R.string.task_manager_task_column);
            } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
                textView = view.findViewById(R.id.memory_footprint);
                textView.setText(R.string.task_manager_mem_footprint_column);
            } else if (columnKey == TaskManagerProperties.CPU) {
                textView = view.findViewById(R.id.cpu);
                textView.setText(R.string.task_manager_cpu_column);
            } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
                textView = view.findViewById(R.id.process_id);
                textView.setText(R.string.task_manager_process_id_column);
            } else {
                throw new IllegalArgumentException("column key " + columnKey + " not supported");
            }

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

    // TODO(crbug.com/380188424): Customize stringification per column type.
    private static void bindTask(PropertyModel model, View view, PropertyKey key) {
        if (key == TaskManagerProperties.TASK_ID) {
            return;
        } else if (key == TaskManagerProperties.IS_SELECTED) {
            if (model.get(TaskManagerProperties.IS_SELECTED)) {
                view.setBackgroundColor(
                        SemanticColorUtils.getColorPrimaryContainer(view.getContext()));
            } else {
                view.setBackgroundColor(0);
            }
        } else if (key == TaskManagerProperties.TASK_NAME) {
            TextView textView = view.findViewById(R.id.task_name);
            textView.setText(model.get(TaskManagerProperties.TASK_NAME));
        } else if (key == TaskManagerProperties.MEMORY_FOOTPRINT) {
            TextView textView = view.findViewById(R.id.memory_footprint);
            textView.setText(String.valueOf(model.get(TaskManagerProperties.MEMORY_FOOTPRINT)));
        } else if (key == TaskManagerProperties.CPU) {
            TextView textView = view.findViewById(R.id.cpu);
            textView.setText(String.valueOf(model.get(TaskManagerProperties.CPU)));
        } else if (key == TaskManagerProperties.PROCESS_ID) {
            TextView textView = view.findViewById(R.id.process_id);
            textView.setText(String.valueOf(model.get(TaskManagerProperties.PROCESS_ID)));
        } else {
            throw new IllegalArgumentException();
        }
    }
}
