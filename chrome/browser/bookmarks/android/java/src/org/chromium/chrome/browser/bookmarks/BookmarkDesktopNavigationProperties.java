// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the bookmark desktop navigation pane. */
@NullMarked
class BookmarkDesktopNavigationProperties {
    public static final WritableObjectPropertyKey<BookmarkId> BOOKMARK_ID =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] FOLDER_KEYS =
            PropertyModel.concatKeys(
                    NavigationPaneProperties.NAVIGATION_ITEM_KEYS, new PropertyKey[] {BOOKMARK_ID});
}
