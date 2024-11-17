// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

/** Provides a static method to create a TaskManager instance. */
public class TaskManagerFactory {
    private TaskManagerFactory() {}

    /**
     * @return a TaskManager instance to launch the task manager.
     */
    public static TaskManager createTaskManager() {
        return new TaskManagerImpl();
    }
}
