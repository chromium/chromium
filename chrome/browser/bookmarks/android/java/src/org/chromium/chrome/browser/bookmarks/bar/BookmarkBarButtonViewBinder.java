// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binder for a button in the bookmark bar which provides users with bookmark access from top
 * chrome.
 */
@NullMarked
class BookmarkBarButtonViewBinder {

    private BookmarkBarButtonViewBinder() {}

    /**
     * Updates a bookmark bar button to reflect the state of a single model property.
     *
     * @param model the model containing the property for which to update the bookmark bar button.
     * @param view the bookmark bar button to update to reflect the state of the model property.
     * @param key the key for the property in the model for which to update the bookmark bar button.
     */
    public static void bind(PropertyModel model, BookmarkBarButton view, PropertyKey key) {
        if (key == BookmarkBarButtonProperties.CLICK_CALLBACK) {
            view.setClickCallback(model.get(BookmarkBarButtonProperties.CLICK_CALLBACK));
        } else if (key == BookmarkBarButtonProperties.KEY_LISTENER) {
            view.setOnKeyListener(model.get(BookmarkBarButtonProperties.KEY_LISTENER));
        } else if (key == BookmarkBarButtonProperties.ICON_SUPPLIER) {
            view.setIconSupplier(model.get(BookmarkBarButtonProperties.ICON_SUPPLIER));
        } else if (key == BookmarkBarButtonProperties.ICON_TINT_LIST_ID) {
            view.setIconTintList(model.get(BookmarkBarButtonProperties.ICON_TINT_LIST_ID));
        } else if (key == BookmarkBarButtonProperties.TITLE) {
            view.setTitle(model.get(BookmarkBarButtonProperties.TITLE));
        } else if (key == BookmarkBarButtonProperties.TOOLTIP) {
            view.setTooltipText(model.get(BookmarkBarButtonProperties.TOOLTIP));
        } else if (key == BookmarkBarButtonProperties.FOLDER_CONTENT_DESCRIPTION) {
            String contentDescription =
                    model.get(BookmarkBarButtonProperties.FOLDER_CONTENT_DESCRIPTION);
            if (contentDescription != null && !contentDescription.isEmpty()) {
                view.setContentDescription(contentDescription);
            }
        } else if (key == BookmarkBarButtonProperties.TEXT_APPEARANCE_ID) {
            int resId = model.get(BookmarkBarButtonProperties.TEXT_APPEARANCE_ID);
            view.setTitleTextAppearance(resId);
        } else if (key == BookmarkBarButtonProperties.BOOKMARK_ITEM) {
            // No-op.
        } else if (key == BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID) {
            view.setBackgroundResource(
                    model.get(BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID));
        }
    }
}
