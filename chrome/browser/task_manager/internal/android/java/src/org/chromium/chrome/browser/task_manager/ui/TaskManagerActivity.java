// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.os.Bundle;
import android.view.View;

import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Entrypoint of the task manager UI. This activity instantiates the underlying model, the mediator
 * that keeps updating the model, and binds the model and the view.
 */
public class TaskManagerActivity extends ChromeBaseAppCompatActivity {
    private static final int REFRESH_TIME_MS = 1000;
    private final PropertyModel mHeaderModel =
            new PropertyModel(TaskManagerProperties.HEADER_PROPERTY_KEYS);
    private final ModelList mTasksModel = new ModelList();
    private final TaskManagerMediator mMediator =
            new TaskManagerMediator(
                    REFRESH_TIME_MS,
                    mHeaderModel,
                    mTasksModel,
                    TaskManagerProperties.TASK_NAME,
                    TaskManagerProperties.MEMORY_FOOTPRINT,
                    TaskManagerProperties.CPU,
                    TaskManagerProperties.NETWORK_USAGE,
                    TaskManagerProperties.PROCESS_ID);
    private TaskManagerCoordinator mCoordinator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.task_manager_activity);
        View taskManagerView = findViewById(android.R.id.content);

        mCoordinator =
                new TaskManagerCoordinator(taskManagerView, mHeaderModel, mTasksModel, mMediator);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mCoordinator.destroy();
    }
}
