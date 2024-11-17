// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Log;

public class TaskManagerActivity extends AppCompatActivity {
    private static final String TAG = "TaskManager";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "onCreate");

        // Feel free to remove the following code on implementing the real task manager UI.
        setContentView(R.layout.task_manager_activity);
        TextView textView = findViewById(R.id.five);

        TaskManagerServiceBridge bridge = new TaskManagerServiceBridge();
        int five = bridge.getFive();

        textView.setText(String.valueOf(five));
    }
}
