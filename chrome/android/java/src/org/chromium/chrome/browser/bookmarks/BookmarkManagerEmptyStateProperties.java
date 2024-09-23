// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Responsible for hosting properties of BookmarkManager empty state. */
class BookmarkManagerEmptyStateProperties {
    static final WritableIntPropertyKey EMPTY_STATE_TITLE_RES = new WritableIntPropertyKey();
    static final WritableIntPropertyKey EMPTY_STATE_DESCRIPTION_RES = new WritableIntPropertyKey();
    static final WritableIntPropertyKey EMPTY_STATE_IMAGE_RES = new WritableIntPropertyKey();

    static final PropertyKey[] EMPTY_STATE_KEYS = {
        EMPTY_STATE_TITLE_RES, EMPTY_STATE_DESCRIPTION_RES, EMPTY_STATE_IMAGE_RES
    };

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(BookmarkManagerProperties.ALL_KEYS, EMPTY_STATE_KEYS);
}
