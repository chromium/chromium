// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

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
                mBridge.addObserver(observer, REFRESH_TIME_MS, RefreshType.MEMORY_FOOTPRINT);
    }

    private TaskManagerObserver createObserver() {
        // Feel free to update the following code on implementing the real task manager UI.

        TextView textView = findViewById(R.id.five);

        Map<Long, Long> mem = new TreeMap<Long, Long>();
        return new TaskManagerObserver() {
            @Override
            public void onTaskAdded(long id) {}

            @Override
            public void onTaskToBeRemoved(long id) {}

            @Override
            public void onTasksRefreshed(long[] taskIds) {
                for (long taskId : taskIds) {
                    mem.put(taskId, mBridge.getMemoryFootprintUsage(taskId));
                }
                StringBuilder txt = new StringBuilder();
                for (Map.Entry<Long, Long> e : mem.entrySet()) {
                    txt.append("Task ");
                    txt.append(e.getKey());
                    txt.append("'s memory footprint usage = ");
                    txt.append(e.getValue());
                    txt.append("\n");
                }
                textView.setText(txt.toString());
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
