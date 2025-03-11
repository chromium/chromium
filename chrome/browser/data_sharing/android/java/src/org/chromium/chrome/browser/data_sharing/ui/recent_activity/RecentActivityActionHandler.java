// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for handling click events on recent activity rows. Depending on the type of the
 * activity row, one of the methods listed in the interface will be invoked.
 */
@NullMarked
public interface RecentActivityActionHandler {
    /** Called to focus a tab. Invoked for tab added / updated events. */
    void focusTab(int tabId);

    /** Called to reopen a tab. Invoked for tab deleted events. */
    void reopenTab(String url);

    /**
     * Called to open the tab group dialog to edit tab group name or color. Invoked for tab group
     * name / color change events.
     */
    void openTabGroupEditDialog();

    /**
     * Called to open the shared tab group member management screen. Invoked for people group
     * related events.
     */
    void manageSharing();
}
