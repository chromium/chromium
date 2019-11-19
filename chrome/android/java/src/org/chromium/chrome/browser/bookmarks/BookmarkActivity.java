// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * The activity that displays the bookmark UI on the phone. It keeps a {@link BookmarkManager}
 * inside of it and creates a snackbar manager. This activity should only be shown on phones; on
 * tablet the bookmark UI is shown inside of a tab (see {@link BookmarkPage}).
 */
public class BookmarkActivity extends SnackbarActivity {

    private BookmarkManager mBookmarkManager;
    static final int EDIT_BOOKMARK_REQUEST_CODE = 14;
    public static final String INTENT_VISIT_BOOKMARK_ID = "BookmarkEditActivity.VisitBookmarkId";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mBookmarkManager = new BookmarkManager(this, true, getSnackbarManager());
        String url = getIntent().getDataString();
        if (TextUtils.isEmpty(url)) url = UrlConstants.BOOKMARKS_URL;
        mBookmarkManager.updateForUrl(url);
        setContentView(mBookmarkManager.getView());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mBookmarkManager.onDestroyed();
    }

    @Override
    public void onBackPressed() {
        if (!mBookmarkManager.onBackPressed()) super.onBackPressed();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == EDIT_BOOKMARK_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId bookmarkId = BookmarkId.getBookmarkIdFromString(data.getStringExtra(
                    INTENT_VISIT_BOOKMARK_ID));
            mBookmarkManager.openBookmark(bookmarkId, BookmarkLaunchLocation.BOOKMARK_EDITOR);
        }
    }

    /**
     * @return The {@link BookmarkManager} for testing purposes.
     */
    @VisibleForTesting
    public BookmarkManager getManagerForTesting() {
        return mBookmarkManager;
    }
}
