// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBarViewBinder {

    private BookmarkBarViewBinder() {}

    /**
     * Updates the bookmark bar to reflect the state of a single model property.
     *
     * @param model the model containing the property for which to update the bookmark bar.
     * @param view the bookmark bar to update to reflect the state of the model property.
     * @param key the key for the property in the model for which to update the bookmark bar.
     */
    public static void bind(PropertyModel model, BookmarkBar view, PropertyKey key) {
        if (key == BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK) {
            view.setHeightChangeCallback(model.get(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK));
        } else if (key == BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK) {
            view.setOverflowButtonClickCallback(
                    model.get(BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK));
        } else if (key == BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY) {
            view.setOverflowButtonVisibility(
                    model.get(BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY));
        } else if (key == BookmarkBarProperties.TOP_MARGIN) {
            final var lp = (MarginLayoutParams) view.getLayoutParams();
            lp.topMargin = model.get(BookmarkBarProperties.TOP_MARGIN);
            view.setLayoutParams(lp);
        } else if (key == BookmarkBarProperties.VISIBILITY) {
            view.setVisibility(model.get(BookmarkBarProperties.VISIBILITY));
        }
    }
}
