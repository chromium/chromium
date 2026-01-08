// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

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
}
