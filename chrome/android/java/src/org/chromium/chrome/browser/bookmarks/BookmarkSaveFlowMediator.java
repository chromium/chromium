// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.PropertyModel;

/** Controls the bookmarks save-flow. */
public class BookmarkSaveFlowMediator extends BookmarkModelObserver {
    private final Context mContext;
    private final Runnable mCloseRunnable;
    private PropertyModel mPropertyModel;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mBookmarkId;

    /**
     * @param bookmarkModel The {@link BookmarkModel} which supplies the data.
     * @param propertyModel The {@link PropertyModel} which allows the mediator to push data to the
     *         model.
     * @param context The {@link Context} associated with this mediator.
     * @param closeRunnable A {@link Runnable} which closes the bookmark save flow.
     */
    public BookmarkSaveFlowMediator(BookmarkModel bookmarkModel, PropertyModel propertyModel,
            Context context, Runnable closeRunnable) {
        mBookmarkModel = bookmarkModel;
        mPropertyModel = propertyModel;
        mContext = context;
        mCloseRunnable = closeRunnable;
        mBookmarkModel.addObserver(this);
    }

    /**
     * Shows bottom sheet save-flow for the given {@link BookmarkId}.
     *
     * @param bookmarkId The {@link BookmarkId} to show.
     */
    public void show(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        RecordUserAction.record("MobileBookmark.SaveFlow.Show");

        bindBookmarkProperties(mBookmarkId);
        mPropertyModel.set(BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditBookmark");
            BookmarkUtils.startEditActivity(mContext, mBookmarkId);
            mCloseRunnable.run();
        });
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditFolder");
            BookmarkUtils.startFolderSelectActivity(mContext, mBookmarkId);
            mCloseRunnable.run();
        });
    }

    private void bindBookmarkProperties(BookmarkId bookmarkId) {
        BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
        String folderName = mBookmarkModel.getBookmarkTitle(item.getParentId());
        mPropertyModel.set(BookmarkSaveFlowProperties.TITLE_TEXT,
                BookmarkUtils.getSaveFlowTitleForBookmark(mContext, bookmarkId));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON,
                BookmarkUtils.getFolderIcon(mContext, bookmarkId.getType()));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED, item.isMovable());
        mPropertyModel.set(BookmarkSaveFlowProperties.SUBTITLE_TEXT,
                mContext.getResources().getString(
                        R.string.bookmark_page_saved_location, folderName));
    }

    void destroy() {
        mBookmarkModel.removeObserver(this);
        mBookmarkModel = null;
        mPropertyModel = null;
        mBookmarkId = null;
    }

    // BookmarkModelObserver implementation

    @Override
    public void bookmarkModelChanged() {
        if (mBookmarkId == null) return;
        bindBookmarkProperties(mBookmarkId);
    }
}
