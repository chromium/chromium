// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.util.Pair;

import androidx.annotation.NonNull;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Encapsulates the logic of showing the folder picker and observing the result. Specifically: -
 * Launch the BookmarkFolderPicker activity. - Observing any move events in the BookmarkModel. -
 * Showing a snackbar for those move events with the proper messaging.
 *
 * <p>Note: This class doesn't do much when the account bookmarks feature is disabled.
 *
 * <p>Note: This class can be used for multiple launches of BookmarkFolderPickerActivity.
 */
public class BookmarkMoveSnackbarManager implements ActivityStateListener {
    static final int FOLDER_CHARACTER_LIMIT = 32;
    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkNodeMoved(
                        BookmarkItem oldParent,
                        int oldIndex,
                        BookmarkItem newParent,
                        int newIndex) {
                    if (!mIsObserving) {
                        return;
                    }

                    // TODO(crbug.com/41496270): Consider handling the edge cases here where one or
                    // multiple bookmarks fail to move. For now, just roll with any of the bookmarks
                    // being moved.
                    String message;
                    String folderTitle = getShortenedFolderTitle(newParent);
                    Resources res = mContext.getResources();
                    if (newParent.isAccountBookmark()) {
                        String accountEmail =
                                mIdentityManager
                                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                                        .getEmail();
                        message =
                                res.getQuantityString(
                                        R.plurals.account_bookmark_move_snackbar_message,
                                        mBookmarkIds.size(),
                                        folderTitle,
                                        accountEmail);
                    } else {
                        message =
                                res.getQuantityString(
                                        R.plurals.local_bookmark_move_snackbar_message,
                                        mBookmarkIds.size(),
                                        folderTitle);
                    }

                    mPendingSnackbar =
                            Snackbar.make(
                                            message,
                                            new SnackbarController() {},
                                            Snackbar.TYPE_NOTIFICATION,
                                            Snackbar.UMA_BOOKMARK_MOVED)
                                    .setSingleLine(false);
                    mIsObserving = false;
                }

                @Override
                public void bookmarkModelChanged() {}
            };

    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final SnackbarManager mSnackbarManager;
    private final IdentityManager mIdentityManager;

    private List<BookmarkId> mBookmarkIds;
    private List<Pair<String, GURL>> mBookmarkTitlesAndUrls;
    private boolean mIsObserving;
    private Snackbar mPendingSnackbar;

    public BookmarkMoveSnackbarManager(
            @NonNull Context context,
            @NonNull BookmarkModel bookmarkModel,
            @NonNull SnackbarManager snackbarManager,
            @NonNull IdentityManager identityManager) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mSnackbarManager = snackbarManager;
        mIdentityManager = identityManager;
        ApplicationStatus.registerStateListenerForAllActivities(this);
    }

    /** Called when the BookmarkMoveSnackbarManager is no longer needed. */
    public void destroy() {
        ApplicationStatus.unregisterActivityStateListener(this);
        mBookmarkModel.removeObserver(mBookmarkModelObserver);
    }

    /**
     * Starts the folder picker and observes the resulting moves for the given bookmarkIds.
     *
     * @param bookmarkIds The {@link BookmarkId} that are being moved.
     */
    public void startFolderPickerAndObserveResult(BookmarkId... bookmarkIds) {
        // Snackbars will only be shown when the feature is enabled.
        mIsObserving = mBookmarkModel.areAccountBookmarkFoldersActive();
        mBookmarkIds = Arrays.asList(bookmarkIds);

        // TODO(crbug.com/1465757): Record user action.
        BookmarkUtils.startFolderPickerActivity(mContext, bookmarkIds);
    }

    // ActivityStateListener implementation.

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        // It's possible that the activity was closed without anything being moved. In that case,
        // stop observing and wait for the next call to {@link startFolderPickerAndObserveResult}.
        if (activity instanceof BookmarkFolderPickerActivity
                && newState == ActivityState.DESTROYED) {
            mIsObserving = false;
        }

        // This will only happen when we get back to the calling context which owns this snackbar
        // (e.g. BookmarkEditActivity, BookmarkManager).
        if (mPendingSnackbar != null && mSnackbarManager.canShowSnackbar()) {
            mSnackbarManager.showSnackbar(mPendingSnackbar);
            mPendingSnackbar = null;
        }
    }

    /** Truncates folder titles that are too long so they fit into the snackbar. */
    String getShortenedFolderTitle(BookmarkItem item) {
        String title = item.getTitle();
        if (title.length() > FOLDER_CHARACTER_LIMIT) {
            title = title.substring(0, FOLDER_CHARACTER_LIMIT);
            title += "...";
        }
        return title;
    }

    // Testing-specific methods.

    BookmarkModelObserver getBookmarkModelObserverForTesting() {
        return mBookmarkModelObserver;
    }
}
