// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;

/** Interface for the Tab Groups related UI. */
public interface TabGroupUi extends BottomControlsContentDelegate {
    /**
     * @return Whether the TabGridDialog is visible.
     */
    boolean isTabGridDialogVisible();
}
