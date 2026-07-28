// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties for the bookmark desktop navigation pane. */
@NullMarked
class BookmarkDesktopNavigationProperties {
    // View types
    public static final int NAVIGATION_TYPE_FOLDER = 0;
    public static final int NAVIGATION_TYPE_HEADER = 1;

    // Folder properties
    public static final PropertyModel.WritableObjectPropertyKey<BookmarkId> BOOKMARK_ID =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> ICON =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SELECTED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] FOLDER_KEYS = {
        BOOKMARK_ID, TITLE, ICON, IS_SELECTED, ON_CLICK_HANDLER
    };

    // Header properties
    public static final PropertyModel.WritableObjectPropertyKey<String> HEADER_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] HEADER_KEYS = {HEADER_TITLE};
}
