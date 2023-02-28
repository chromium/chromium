// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
// TODO(crbug.com/1413463): Write unittests for this class.
class BookmarkToolbarMediator implements BookmarkUIObserver {
    private final PropertyModel mModel;

    // TODO(crbug.com/1413463): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    BookmarkToolbarMediator(PropertyModel model) {
        mModel = model;
    }

    void initialize(BookmarkDelegate bookmarkDelegate) {
        mBookmarkDelegate = bookmarkDelegate;
        mBookmarkDelegate.addUIObserver(this);
    }

    // BookmarkUIObserver implementation.

    @Override
    public void onDestroy() {
        if (mBookmarkDelegate != null) {
            mBookmarkDelegate.removeUIObserver(this);
        }
    }

    @Override
    public void onStateChanged(int state) {
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_STATE, state);
    }
}