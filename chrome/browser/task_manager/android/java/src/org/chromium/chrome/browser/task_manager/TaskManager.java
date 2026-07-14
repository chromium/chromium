// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Chrome's task manager is a tool that provides detailed information about the process and
 * resources used by the Chrome browser. It allows the user to monitor and manage these processes
 * including identifying and terminating processes that are consuming excessive resources and
 * causing problems.
 *
 * <p>This interface provides means to launch the task manager.
 */
@NullMarked
public interface TaskManager {
    /** Returns whether the task manager feature is enabled. */
    static boolean isEnabled() {
        // Use of `isDesktop()` here is approved in https://crbug.com/525590366
        return DeviceInfo.isDesktop()
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_CLANK);
    }

    /** Launches the task manager. */
    void launch(Context context);
}
