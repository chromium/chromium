// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkId;

/**
 * Observer interface to get notification for UI mode changes, bookmark changes, and other related
 * event that affects UI. All bookmark UI components are expected to implement this and
 * update themselves correctly on each event.
 */
interface BookmarkUiObserver {
    /** Called when the entire UI is being destroyed and will be no longer in use. */
    default void onDestroy() {}

    /** @see BookmarkDelegate#openFolder(BookmarkId) */
    default void onFolderStateSet(BookmarkId folder) {}

    /** Called when the UI state is set to {@link BookmarkUiState#STATE_SEARCHING}. */
    default void onSearchStateSet() {}

    /** Called when a bookmark menu item is opened. */
    default void onBookmarkItemMenuOpened() {}

    /** Called when the bookmark UI state changes. */
    default void onStateChanged(int state) {}
}
