// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binder for a button in the bookmark bar which provides users with bookmark access from top
 * chrome.
 */
class BookmarkBarButtonViewBinder {

    private BookmarkBarButtonViewBinder() {}

    /**
     * Updates a bookmark bar button to reflect the state of a single model property.
     *
     * @param model the model containing the property for which to update the bookmark bar button.
     * @param view the bookmark bar button to update to reflect the state of the model property.
     * @param key the key for the property in the model for which to update the bookmark bar button.
     */
    public static void bind(
            @NonNull PropertyModel model,
            @NonNull BookmarkBarButton view,
            @NonNull PropertyKey key) {
        if (key == BookmarkBarButtonProperties.CLICK_CALLBACK) {
            final var callback = model.get(BookmarkBarButtonProperties.CLICK_CALLBACK);
            view.setOnClickListener(callback != null ? (v) -> callback.run() : null);
        } else if (key == BookmarkBarButtonProperties.ICON) {
            view.setIcon(model.get(BookmarkBarButtonProperties.ICON));
        } else if (key == BookmarkBarButtonProperties.TITLE) {
            view.setTitle(model.get(BookmarkBarButtonProperties.TITLE));
        }
    }
}
