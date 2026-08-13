// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.search.SearchBoxViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Binds model properties to the bookmark search box row view. */
@NullMarked
public class BookmarkSearchBoxRowViewBinder {
    public static void bind(PropertyModel model, BookmarkSearchBoxRow row, PropertyKey key) {
        if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES) {
            row.setShoppingChipIcon(
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES) {
            row.setShoppingChipText(
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY) {
            row.setChipContainerVisibility(
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK) {
            Callback<Boolean> onToggle =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK);
            if (onToggle == null) {
                row.setShoppingChipToggleListener(null);
            } else {
                row.setShoppingChipToggleListener(
                        (v) -> {
                            boolean isSelected =
                                    model.get(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED);
                            onToggle.onResult(!isSelected);
                        });
            }
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED) {
            row.setShoppingChipSelected(
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED));
        } else {
            SearchBoxViewBinder.bind(model, row.getSearchBoxView(), key);
        }
    }

    public static ViewBinder<PropertyModel, BookmarkSearchBoxRow, PropertyKey> createViewBinder() {
        return BookmarkSearchBoxRowViewBinder::bind;
    }
}
