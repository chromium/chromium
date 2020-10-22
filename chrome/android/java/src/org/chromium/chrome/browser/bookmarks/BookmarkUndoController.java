// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModel.BookmarkDeleteObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.Locale;

/**
 * Shows an undo bar when the user modifies bookmarks, allowing them to undo their changes.
 */
public class BookmarkUndoController extends BookmarkModelObserver implements
        SnackbarManager.SnackbarController, BookmarkDeleteObserver {

    private final BookmarkModel mBookmarkModel;
    private final SnackbarManager mSnackbarManager;
    private final Context mContext;

    /**
     * Creates an instance of {@link BookmarkUndoController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param model The bookmark model.
     * @param snackbarManager SnackManager passed from activity.
     */
    public BookmarkUndoController(Context context, BookmarkModel model,
            SnackbarManager snackbarManager) {
        mBookmarkModel = model;
        mBookmarkModel.addDeleteObserver(this);
        mSnackbarManager = snackbarManager;
        mContext = context;
    }

    /**
     * Cleans up this class, unregistering for application notifications from bookmark model.
     */
    public void destroy() {
        mBookmarkModel.removeDeleteObserver(this);
        mSnackbarManager.dismissSnackbars(this);
    }

    @Override
    public void onAction(Object actionData) {
        mBookmarkModel.undo();
        mSnackbarManager.dismissSnackbars(this);
    }

    @Override
    public void onDismissNoAction(Object actionData) {
    }

    // Overriding BookmarkModelObserver
    @Override
    public void bookmarkModelChanged() {
        mSnackbarManager.dismissSnackbars(this);
    }

    @Override
    public void bookmarkNodeChanged(BookmarkItem node) {
        // Title/url change of a bookmark should not affect undo.
    }

    @Override
    public void bookmarkNodeAdded(BookmarkItem parent, int index) {
        // Adding a new bookmark should not affect undo.
    }

    // Implement BookmarkDeleteObserver
    @Override
    public void onDeleteBookmarks(String[] titles, boolean isUndoable) {
        assert titles != null && titles.length >= 1;

        if (!isUndoable) return;

        if (titles.length == 1) {
            mSnackbarManager.showSnackbar(
                    Snackbar.make(titles[0], this, Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_DELETE_UNDO)
                            .setTemplateText(mContext.getString(R.string.delete_message))
                            .setAction(mContext.getString(R.string.undo), null));
        } else {
            mSnackbarManager.showSnackbar(
                    Snackbar.make(String.format(Locale.getDefault(), "%d", titles.length), this,
                            Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_DELETE_UNDO)
                    .setTemplateText(mContext.getString(R.string.undo_bar_multiple_delete_message))
                    .setAction(mContext.getString(R.string.undo), null));
        }
    }
}
