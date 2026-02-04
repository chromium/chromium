// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.List;

/**
 * Tracks {@link RecentlyClosedEntriesManager}s.
 *
 * <p>The implementation of this interface should be a singleton that maintains an internal
 * collection of all {@link RecentlyClosedEntriesManager}s.
 */
@NullMarked
public interface RecentlyClosedEntriesManagerTracker {
    /**
     * Creates and returns an instance of {@link RecentlyClosedEntriesManager} for the calling
     * activity.
     *
     * @param multiInstanceManager The {@link MultiInstanceManager} instance used to manage recently
     *     closed windows.
     * @param tabModelSelector The {@link TabModelSelector} that owns the tab model to access
     *     restored tabs.
     * @return The {@link RecentlyClosedEntriesManager} instance for the calling activity.
     */
    RecentlyClosedEntriesManager obtainManager(
            MultiInstanceManager multiInstanceManager, TabModelSelector tabModelSelector);

    /**
     * Destroys the specified {@link RecentlyClosedEntriesManager} instance.
     *
     * @param manager The {@link RecentlyClosedEntriesManager} instance to destroy.
     */
    void destroy(RecentlyClosedEntriesManager manager);

    /**
     * Notifies all {@link RecentlyClosedEntriesManager} instances when instances are closed.
     * Closure can be system-initiated (for e.g. low-memory kill), app-initiated (for e.g. instance
     * retention expiration) or user-initiated (for e.g. window manager closure).
     *
     * @param instanceInfo The list of {@link InstanceInfo} for the closed instances.
     * @param isPermanentDeletion Whether the closed instance is permanently deleted.
     */
    void onInstancesClosed(List<InstanceInfo> instanceInfo, boolean isPermanentDeletion);

    /**
     * Notifies all {@link RecentlyClosedEntriesManager} instances when an inactive instance is
     * restored.
     *
     * @param instanceId The ID for the restored instance.
     */
    void onInstanceRestored(int instanceId);
}
