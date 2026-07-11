// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * Orchestrates background actuation for Actor tasks. Coordinates between the Foreground Service and
 * the browser's offscreen rendering components.
 */
@NullMarked
public class ActorBackgroundActuationManager {
    /**
     * Parameters for transitioning a task to the background. Bundles the task identity.
     */
    public static class BackgroundTransitionParams {
        public final int taskId;

        public BackgroundTransitionParams(int taskId) {
            this.taskId = taskId;
        }
    }

    public ActorBackgroundActuationManager() {}

    /**
     * Handles prerequisite needed for initiating an actor task in background.
     *
     * @param prompt The prompt to be executed by the agent, if no cached prompt available.
     */
    public void handleBackgroundStart(@Nullable String prompt) {
        // TODO: Implement this.
    }

    /**
     * Transitions active actor tasks from foreground to background.
     *
     * @param tasks List of tasks to be moved offscreen.
     */
    public void transitionToBackground(List<BackgroundTransitionParams> tasks) {
        // TODO: Implement this.
    }
}
