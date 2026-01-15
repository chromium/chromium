// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.build.annotations.NullMarked;

/** Delegate to handle actions triggered from the tab strip context menu. */
@NullMarked
public interface TabStripContextMenuDelegate {
    /** Called when the "New tab" menu item is selected. */
    void onNewTab();

    /** Called when the "Name window" menu item is selected. */
    void onNameWindow();
}
