// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Entrypoint of the task manager UI. This activity instantiates the underlying model, the mediator
 * that keeps updating the model, and binds the model and the view.
 */
public class TaskManagerActivity extends AppCompatActivity {
    private static final int REFRESH_TIME_MS = 1000;
    private final ModelList mModelList = new ModelList();
    private final TaskManagerMediator mMediator =
            new TaskManagerMediator(
                    REFRESH_TIME_MS,
                    mModelList,
                    TaskManagerProperties.TASK_ID,
                    TaskManagerProperties.TASK_NAME,
                    TaskManagerProperties.MEMORY_FOOTPRINT,
                    TaskManagerProperties.CPU,
                    TaskManagerProperties.PROCESS_ID);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        bind(this, mModelList);

        mMediator.startObserving();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mMediator.stopObserving();
    }

    /** Sets up the UI in the activity, binding the activity and the model. */
    static void bind(Activity activity, ModelList model) {
        activity.setContentView(R.layout.task_manager_activity);

        RecyclerView recyclerView = activity.findViewById(R.id.tasks_view);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false));

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(model);
        recyclerView.setAdapter(adapter);

        adapter.registerType(
                TaskManagerProperties.RowType.HEADER,
                (parent) ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.task_item, parent, false),
                TaskManagerActivity::bindHeader);

        adapter.registerType(
                TaskManagerProperties.RowType.TASK,
                (parent) ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.task_item, parent, false),
                TaskManagerActivity::bindTask);
    }

    // TOOD(crbug.com/380158926): i18n.
    @SuppressLint("SetTextI18n")
    private static void bindHeader(PropertyModel model, View view, PropertyKey key) {
        if (key != TaskManagerProperties.COLUMNS) {
            throw new IllegalArgumentException();
        }
        for (PropertyKey columnKey : model.get(TaskManagerProperties.COLUMNS)) {
            TextView textView;
            if (columnKey == TaskManagerProperties.TASK_ID) {
                continue;
            } else if (columnKey == TaskManagerProperties.TASK_NAME) {
                textView = view.findViewById(R.id.task_name);
                textView.setText("Task");
            } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
                textView = view.findViewById(R.id.memory_footprint);
                textView.setText("Memory footprint");
            } else if (columnKey == TaskManagerProperties.CPU) {
                textView = view.findViewById(R.id.cpu);
                textView.setText("CPU");
            } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
                textView = view.findViewById(R.id.process_id);
                textView.setText("Process ID");
            } else {
                throw new IllegalArgumentException();
            }
            textView.setTypeface(null, Typeface.BOLD);
        }
    }

    // TODO(crbug.com/380188424): Customize stringification per column type.
    private static void bindTask(PropertyModel model, View view, PropertyKey key) {
        if (key == TaskManagerProperties.TASK_ID) {
            return;
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
