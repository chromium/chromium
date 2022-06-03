// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder to connect each bookmark bottom sheet row to {@link PropertyModel}.
 */
class BookmarkBottomSheetRowViewBinder {
    static void bind(
            PropertyModel model, BookmarkBottomSheetFolderRow view, PropertyKey propertyKey) {
        if (BookmarkBottomSheetItemProperties.TITLE.equals(propertyKey)) {
            view.setTitle(model.get(BookmarkBottomSheetItemProperties.TITLE));
        } else if (BookmarkBottomSheetItemProperties.SUBTITLE.equals(propertyKey)) {
            view.setSubtitle(model.get(BookmarkBottomSheetItemProperties.SUBTITLE));
        } else if (BookmarkBottomSheetItemProperties.ICON_DRAWABLE_AND_COLOR.equals(propertyKey)) {
            view.setIcon(model.get(BookmarkBottomSheetItemProperties.ICON_DRAWABLE_AND_COLOR));
        } else if (BookmarkBottomSheetItemProperties.ON_CLICK_LISTENER.equals(propertyKey)) {
            view.setOnClickListener(model.get(BookmarkBottomSheetItemProperties.ON_CLICK_LISTENER));
        }
    }

    private BookmarkBottomSheetRowViewBinder() {}
}
