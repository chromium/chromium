// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.content.Context;

/**
 * Chrome's task manager is a tool that provides detailed information about the process and
 * resources used by the Chrome browser. It allows the user to monitor and manage these processes
 * including identifying and terminating processes that are consuming excessive resources and
 * causing problems.
 *
 * <p>This interface provides means to launch the task manager.
 */
public interface TaskManager {
    /** Launches the task manager. */
    void launch(Context context);
}
