// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

public class TaskManagerActivity extends AppCompatActivity {
    private static final String TAG = "TaskManager";
    private static final int REFRESH_TIME_MS = 1000;

    private TaskManagerServiceBridge mBridge = new TaskManagerServiceBridge();
    private TaskManagerServiceBridge.ObserverHandle mObserverHandle;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.task_manager_activity);

        TaskManagerObserver observer = createObserver();

        mObserverHandle =
                mBridge.addObserver(
                        observer, REFRESH_TIME_MS, RefreshType.MEMORY_FOOTPRINT | RefreshType.CPU);
    }

    private TaskManagerObserver createObserver() {
        // Feel free to update the following code on implementing the real task manager UI.

        TextView textView = findViewById(R.id.five);

        Map<Long, String> rows = new TreeMap<Long, String>();
        return new TaskManagerObserver() {
            @Override
            public void onTaskAdded(long id) {}

            @Override
            public void onTaskToBeRemoved(long id) {
                rows.remove(id);
                onTasksRefreshed(new long[] {});
            }

            @Override
            public void onTasksRefreshed(long[] taskIds) {
                for (long taskId : taskIds) {
                    rows.put(
                            taskId,
                            String.format(
                                    Locale.US,
                                    "Task %d [%s] memory footprint = %d CPU = %.1f pid = %d",
                                    taskId,
                                    mBridge.getTitle(taskId),
                                    mBridge.getMemoryFootprintUsage(taskId),
                                    mBridge.getPlatformIndependentCpuUsage(taskId),
                                    mBridge.getProcessId(taskId)));
                }
                textView.setText(String.join("\n", rows.values()));
            }

            @Override
            public void onTasksRefreshedWithBackgroundCalculations(long[] taskIds) {}

            @Override
            public void onTaskUnresponsive(long id) {}

            @Override
            public void onActiveTaskFetched(long id) {}
        };
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mObserverHandle != null) {
            mBridge.removeObserver(mObserverHandle);
            mObserverHandle = null;
        }
    }
}
