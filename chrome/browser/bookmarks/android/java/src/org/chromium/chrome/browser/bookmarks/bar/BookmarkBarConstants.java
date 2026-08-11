// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import org.chromium.build.annotations.NullMarked;

/** Constants for the Bookmark Bar feature. */
@NullMarked
public class BookmarkBarConstants {

    /**
     * Bookmark Bar preference, tracks whether or not the user wants to show the bookmark bar on the
     * current device.
     */
    public static final String BOOKMARK_BAR_SHOW_BOOKMARK_BAR =
            "Chrome.BookmarkBar.ShowBookmarkBar";

    /**
     * Bookmark Bar tri-state preference, tracks whether or not the user wants to show the bookmark
     * bar on the current device, whether it's always show, only show on NTP, or always hide.
     */
    public static final String BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE =
            "Chrome.BookmarkBar.BookmarkBarVisibilityState";
}
