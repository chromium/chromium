// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.chrome.browser.tab.Tab;

/** Interface for selecting a tab. */
public interface OnTabSelectingListener {
    /**
     * Called when a tab is getting selected. This will select the tab and exit the layout.
     *
     * @param tabId The ID of selected {@link Tab}.
     */
    void onTabSelecting(int tabId);
}
