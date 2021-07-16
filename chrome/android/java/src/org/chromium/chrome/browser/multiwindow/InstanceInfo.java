// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

/**
 * Struct containing the info of ChromeTabbedActivity instance needed to manage
 * multi-instance support on Android S.
 */
final class InstanceInfo {
    /**
     * ID of ChromeTabbedActivity instance. This is compatible with the index used for
     * persistent tab state disk file, appended at the end of the file name (such as tab_state0).
     */
    public final int instanceId;

    /**
     * ID of a task containing the activity.
     */
    public final int taskId;

    public InstanceInfo(int instanceId, int taskId) {
        this.instanceId = instanceId;
        this.taskId = taskId;
    }

    @Override
    public String toString() {
        return "instance-id: " + instanceId + " task-id: " + taskId;
    }
}
