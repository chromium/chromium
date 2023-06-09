// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Responsible for hosting properties for {@link BookmarkSearchBoxRow}. */
class BookmarkSearchBoxRowProperties {
    public static final ReadableObjectPropertyKey<Callback<String>> QUERY_CALLBACK =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
            BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, QUERY_CALLBACK};
}
