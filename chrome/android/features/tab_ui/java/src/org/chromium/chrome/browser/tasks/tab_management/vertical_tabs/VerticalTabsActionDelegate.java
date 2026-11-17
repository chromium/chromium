// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import org.chromium.build.annotations.NullMarked;

/** The delegate to provide actions for the vertical tabs. */
@NullMarked
public interface VerticalTabsActionDelegate {
    /** Opens the tab search overlay side panel. */
    void openTabSearch();

    /**
     * Opens the grid tab switcher (Hub) with its search bar focused. Used as a fallback when the
     * tab search overlay is unavailable and the grid tab switcher is still available.
     */
    void openHubSearch();
}
