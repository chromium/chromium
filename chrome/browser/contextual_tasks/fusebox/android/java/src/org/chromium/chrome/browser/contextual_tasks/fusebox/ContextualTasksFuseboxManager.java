// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.fusebox;

import android.view.View;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages the lifecycle and visibility of a single {@link ContextualTasksFusebox} instance overlaid
 * on regular tabs.
 */
@NullMarked
public interface ContextualTasksFuseboxManager {
    static final UnownedUserDataKey<ContextualTasksFuseboxManager> KEY =
            new UnownedUserDataKey<ContextualTasksFuseboxManager>();

    /**
     * Helper method to retrieve the {@link ContextualTasksFuseboxManager} instance from a given
     * {@link WindowAndroid}.
     *
     * @param windowAndroid The window to retrieve the manager from.
     * @return The manager for the given window, or null if none exists.
     */
    static @Nullable ContextualTasksFuseboxManager from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Called when the WebUI is ready.
     *
     * @param taskId The ID of the task.
     * @param webContents The WebContents of the contextual tasks WebUI.
     */
    void onWebUIReady(String taskId, WebContents webContents);

    /**
     * Called when the WebUI controller is destroyed.
     *
     * @param taskId The ID of the task.
     */
    void onWebUIDestroyed(String taskId);

    /**
     * Called when the task ID is updated inside the WebUI. This normally happens on resumption of a
     * historical thread from inside the WebUI page. Re-keys the session map.
     *
     * @param oldTaskId The old ID of the task.
     * @param newTaskId The new ID of the task.
     */
    void onTaskChanged(String oldTaskId, String newTaskId);

    /** Returns the fusebox view. */
    @Nullable View getFuseboxView();

    /** Returns the {@link ContextualTasksFuseboxDataProvider}. One per activity. */
    ContextualTasksFuseboxDataProvider getFuseboxDataProvider();

    /** Destroys the manager. Called when the activity is destroyed. */
    void destroy();
}
