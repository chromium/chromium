// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.graphics.drawable.Drawable;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu bookmark items. */
@NullMarked
public class AppMenuBookmarkItemProperties {
    public static final WritableObjectPropertyKey<BookmarkId> BOOKMARK_ID =
            new WritableObjectPropertyKey<>("BOOKMARK_ID");

    /** The supplier for the icon for the menu item. */
    public static final WritableObjectPropertyKey<LazyOneshotSupplier<Drawable>> ICON_SUPPLIER =
            new WritableObjectPropertyKey<>("ICON_SUPPLIER");

    public static final PropertyKey[] BOOKMARKS_KEYS =
            new PropertyKey[] {BOOKMARK_ID, ICON_SUPPLIER};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + BOOKMARKS_KEYS.length);

    static {
        for (int i = 0; i < BOOKMARKS_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = BOOKMARKS_KEYS[i];
        }
    }
}
