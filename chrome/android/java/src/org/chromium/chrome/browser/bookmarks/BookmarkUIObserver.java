// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;

/**
 * Observer interface to get notification for UI mode changes, bookmark changes, and other related
 * event that affects UI. All bookmark UI components are expected to implement this and
 * update themselves correctly on each event.
 */
interface BookmarkUIObserver extends SelectionObserver<BookmarkId> {
    /**
     * Called when the entire UI is being destroyed and will be no longer in use.
     */
    void onDestroy();

    /**
     * @see BookmarkDelegate#openFolder(BookmarkId)
     */
    void onFolderStateSet(BookmarkId folder);

    /**
     * Called when the UI state is set to {@link BookmarkUIState#STATE_SEARCHING}.
     */
    void onSearchStateSet();
}
