// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import java.util.Set;

/** Interface for the notification handler for tab group share related components. */
public interface TabListNotificationHandler {
    /**
     * Update the notification bubble indicating a new status for all tab strip items that are part
     * of a shared tab group.
     *
     * @param tabIdsToBeUpdated The set of tab ids that require an update.
     * @param hasUpdate Whether the tab items should show a new update status or not.
     */
    void updateTabStripNotificationBubble(Set<Integer> tabIdsToBeUpdated, boolean hasUpdate);
}
