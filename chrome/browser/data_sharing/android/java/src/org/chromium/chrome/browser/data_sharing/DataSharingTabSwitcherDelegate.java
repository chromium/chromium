// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

/** An interface to handle actions on the TabSwitcher. */
public interface DataSharingTabSwitcherDelegate {
    /**
     * Open the tab group dialog of the given tab group id.
     *
     * @param id The tabId of the first tab in the group.
     */
    public void openTabGroupWithTabId(int tabId);
}
