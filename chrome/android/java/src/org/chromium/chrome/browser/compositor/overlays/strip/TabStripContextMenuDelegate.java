// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;

/** Delegate to handle actions triggered from the tab strip context menu. */
@NullMarked
public interface TabStripContextMenuDelegate {

    /** Called when the "New tab" menu item is selected. */
    void onNewTab();

    /** Returns the type of the most recently closed entry to determine the menu item title. */
    @RecentlyClosedEntryType
    int getRecentlyClosedEntryType();

    /** Called when the "Reopen closed tab/tabs/group" menu item is selected. */
    void onReopenClosedEntry();

    /** Returns the number of tabs in the current tab model. */
    int getTabCount();

    /** Called when the "Bookmark all tabs" menu item is selected. */
    void onBookmarkAllTabs();

    /** Called when the "Name window" menu item is selected. */
    void onNameWindow();

    /** Called when the "Pin Gemini" menu item is selected. */
    void onPinGlic();

    /** Called when the "Unpin Gemini" menu item is selected. */
    void onUnpinGlic();
}
