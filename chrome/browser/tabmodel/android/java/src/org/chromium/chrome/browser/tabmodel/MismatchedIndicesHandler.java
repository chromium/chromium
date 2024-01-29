// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.app.ActivityManager;

/**
 * Contains methods for activities to handle scenarios where a new activity attempts to use a
 * window/tab model selector index that is already assigned, thereby leading to potentially
 * erroneous circumstances.
 */
public interface MismatchedIndicesHandler {

    /**
     * Allows a new activity to perform explicit operations on a previously launched activity in a
     * scenario where both activities are attempting to use the same index.
     *
     * @param activityAtRequestedIndex The activity that launched first and is using the requested
     *     index.
     * @param isActivityInAppTasks Whether the activity that launched first is present in {@link
     *     ActivityManager#getAppTasks()}.
     * @param isActivityInSameTask Whether the activity that launched first is in the same task as
     *     the new activity.
     * @return Whether the operations were successfully performed.
     */
    boolean handleMismatchedIndices(
            Activity activityAtRequestedIndex,
            boolean isActivityInAppTasks,
            boolean isActivityInSameTask);
}
