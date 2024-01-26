// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

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
     * @param preLaunchedActivity The activity that launched first.
     * @return Whether the operations were successfully performed.
     */
    boolean handleMismatchedIndices(Activity preLaunchedActivity);
}
