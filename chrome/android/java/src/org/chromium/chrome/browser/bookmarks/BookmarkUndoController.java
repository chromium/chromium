// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel.BookmarkDeleteObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkItem;

import java.util.Locale;

/** Shows an undo bar when the user modifies bookmarks, allowing them to undo their changes. */
// TODO(crbug.com/40900777): Write tests for this class.
public class BookmarkUndoController extends BookmarkModelObserver
        implements SnackbarManager.SnackbarController, BookmarkDeleteObserver {
    private static final int SNACKBAR_DURATION_MS = 3000;

    private final BookmarkModel mBookmarkModel;
    private final SnackbarManager mSnackbarManager;
    private final Context mContext;
    private final DestroyChecker mDestroyChecker;
    private final boolean mDestroyAfterFirstAction;

    /**
     * Creates an instance of {@link BookmarkUndoController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param model The bookmark model.
     * @param snackbarManager SnackManager passed from activity.
     */
    public BookmarkUndoController(
            Context context, BookmarkModel model, SnackbarManager snackbarManager) {
        this(context, model, snackbarManager, /* destroyAfterFirstAction= */ false);
    }

    /**
     * Internal constructor which specifies an additional parameter.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param model The bookmark model.
     * @param snackbarManager SnackManager passed from activity.
     * @param destroyAfterFirstAction Destroy the controller after the first action.
     */
    private BookmarkUndoController(
            Context context,
            BookmarkModel model,
            SnackbarManager snackbarManager,
            boolean destroyAfterFirstAction) {
        mBookmarkModel = model;
        mBookmarkModel.addDeleteObserver(this);
        mSnackbarManager = snackbarManager;
        mContext = context;
        mDestroyChecker = new DestroyChecker();
        mDestroyAfterFirstAction = destroyAfterFirstAction;
    }

    /** Destroy the object if it hasn't been destroyed already. */
    private void destroyIfNecessary() {
        if (!mDestroyChecker.isDestroyed()) destroy();
    }

    /** Cleans up this class, unregistering for application notifications from bookmark model. */
    public void destroy() {
        mBookmarkModel.removeDeleteObserver(this);
        mSnackbarManager.dismissSnackbars(this);

        mDestroyChecker.destroy();
    }

    public void setEnabled(boolean enabled) {
        if (enabled) {
            mBookmarkModel.addDeleteObserver(this);
        } else {
            mSnackbarManager.dismissSnackbars(this);
            mBookmarkModel.removeDeleteObserver(this);
        }
    }

    @Override
    public void onAction(Object actionData) {
        mDestroyChecker.checkNotDestroyed();

        mBookmarkModel.undo();
        mSnackbarManager.dismissSnackbars(this);
        if (mDestroyAfterFirstAction) destroy();
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        mDestroyChecker.checkNotDestroyed();

        if (mDestroyAfterFirstAction) destroyIfNecessary();
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

    // BookmarkDeleteObserver implementation.

    @Override
    public void onDeleteBookmarks(String[] titles, boolean isUndoable) {
        assert titles != null && titles.length >= 1;

        Snackbar snackbar;
        if (titles.length == 1) {
            snackbar =
                    Snackbar.make(
                                    titles[0],
                                    this,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_DELETE_UNDO)
                            .setTemplateText(mContext.getString(R.string.delete_message));
        } else {
            snackbar =
                    Snackbar.make(
                                    String.format(Locale.getDefault(), "%d", titles.length),
                                    this,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_DELETE_UNDO)
                            .setTemplateText(
                                    mContext.getString(R.string.undo_bar_multiple_delete_message));
        }

        if (isUndoable) {
            snackbar.setAction(mContext.getString(R.string.undo), null);
        }

        mSnackbarManager.showSnackbar(snackbar.setDuration(SNACKBAR_DURATION_MS));
    }
}
