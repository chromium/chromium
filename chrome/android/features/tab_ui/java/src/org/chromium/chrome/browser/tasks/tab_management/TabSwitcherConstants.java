// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/** Constants for tab switcher. */
public class TabSwitcherConstants {
    /** Time to wait before soft cleanup. Soft cleanup will clear tab thumbnails. */
    public static final long SOFT_CLEANUP_DELAY_MS = 3_000L;

    /**
     * Time to wait before performing a hard cleanup. Hard cleanup will log price tracking
     * information and clear the {@link TabListCoordinator} recycler view model.
     */
    public static final long HARD_CLEANUP_DELAY_MS = 30_000L;

    /** Time to wait before destroying the {@link TabSwitcherPaneCoordinator}. */
    public static final long DESTROY_COORDINATOR_DELAY_MS = 60_000L;

    private TabSwitcherConstants() {}
}
