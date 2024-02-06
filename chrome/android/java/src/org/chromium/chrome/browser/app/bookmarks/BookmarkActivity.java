// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * The activity that displays the bookmark UI on the phone. It keeps a {@link
 * BookmarkManagerCoordinator} inside of it and creates a snackbar manager. This activity should
 * only be shown on phones; on tablet the bookmark UI is shown inside of a tab (see {@link
 * BookmarkPage}).
 */
public class BookmarkActivity extends SnackbarActivity {
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    public static final int EDIT_BOOKMARK_REQUEST_CODE = 14;
    public static final String INTENT_VISIT_BOOKMARK_ID = "BookmarkEditActivity.VisitBookmarkId";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        boolean isIncognito =
                IntentUtils.safeGetBooleanExtra(
                        getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);
        Profile profile = ProfileProvider.getOrCreateProfile(getProfileProvider(), isIncognito);
        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        this,
                        IntentUtils.safeGetParcelableExtra(
                                getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT),
                        true,
                        getSnackbarManager(),
                        profile,
                        new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()));
        String url = getIntent().getDataString();
        if (TextUtils.isEmpty(url)) url = UrlConstants.BOOKMARKS_URL;
        mBookmarkManagerCoordinator.updateForUrl(url);
        setContentView(mBookmarkManagerCoordinator.getView());
        BackPressHelper.create(
                this,
                getOnBackPressedDispatcher(),
                mBookmarkManagerCoordinator,
                SecondaryActivity.BOOKMARK);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mBookmarkManagerCoordinator.onDestroyed();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == EDIT_BOOKMARK_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId bookmarkId =
                    BookmarkId.getBookmarkIdFromString(
                            data.getStringExtra(INTENT_VISIT_BOOKMARK_ID));
            mBookmarkManagerCoordinator.openBookmark(bookmarkId);
        }
    }

    /**
     * @return The {@link BookmarkManagerCoordinator} for testing purposes.
     */
    public BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }
}
