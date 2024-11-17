// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;

public class TaskManagerImpl implements TaskManager {
    private static final String TAG = "TaskManager";

    TaskManagerImpl() {}

    @Override
    public void launch(Context context) {
        Log.i(TAG, "launch");

        Intent intent = new Intent(context, TaskManagerActivity.class);

        intent.addFlags(
                Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT
                        | Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        context.startActivity(intent);
    }
}
