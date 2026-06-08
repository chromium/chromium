// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.hub.PaneId;

/** The delegate to provide actions for the vertical tabs. */
@NullMarked
public interface VerticalTabsActionDelegate {
    /**
     * Opens the Hub layout and displays a specific pane.
     *
     * @param paneId The id of the pane to show.
     */
    void openHubPane(@PaneId int paneId);
}
