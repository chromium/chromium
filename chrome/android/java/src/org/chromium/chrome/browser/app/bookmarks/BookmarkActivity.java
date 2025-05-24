// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.content.ComponentName;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The activity that displays the bookmark UI on the phone. It keeps a {@link
 * BookmarkManagerCoordinator} inside of it and creates a snackbar manager. This activity should
 * only be shown on phones; on tablet the bookmark UI is shown inside of a tab (see {@link
 * BookmarkPage}).
 */
public class BookmarkActivity extends SnackbarActivity {
    public static final int EDIT_BOOKMARK_REQUEST_CODE = 14;
    public static final String INTENT_VISIT_BOOKMARK_ID = "BookmarkEditActivity.VisitBookmarkId";

    private @Nullable BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private @Nullable BookmarkOpener mBookmarkOpener;

    @Override
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        @Nullable ComponentName parentComponent =
                IntentUtils.safeGetParcelableExtra(
                        getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mBookmarkOpener =
                new BookmarkOpenerImpl(
                        () -> BookmarkModel.getForProfile(profile),
                        /* context= */ this,
                        /* componentName= */ parentComponent);
        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        this,
                        true,
                        getSnackbarManager(),
                        profile,
                        new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                        mBookmarkOpener,
                        new BookmarkManagerOpenerImpl(),
                        PriceDropNotificationManagerFactory.create(profile));
        String url = getIntent().getDataString();
        if (TextUtils.isEmpty(url)) url = UrlConstants.BOOKMARKS_URL;
        mBookmarkManagerCoordinator.updateForUrl(url);
        setContentView(mBookmarkManagerCoordinator.getView());
        BackPressHelper.create(this, getOnBackPressedDispatcher(), mBookmarkManagerCoordinator);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mBookmarkManagerCoordinator != null) {
            mBookmarkManagerCoordinator.onDestroyed();
        }

        if (mBookmarkOpener != null) {
            mBookmarkOpener.destroy();
            mBookmarkOpener = null;
        }
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

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    /**
     * @return The {@link BookmarkManagerCoordinator} for testing purposes.
     */
    public BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }
}
