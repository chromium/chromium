// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for {@link BookmarkSearchBoxRow}. */
class BookmarkSearchBoxRowViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        BookmarkSearchBoxRow row = (BookmarkSearchBoxRow) view;
        if (key == BookmarkSearchBoxRowProperties.QUERY_CALLBACK) {
            row.setQueryCallback(model.get(BookmarkSearchBoxRowProperties.QUERY_CALLBACK));
        }
    }
}
