// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Business logic for the improved bookmark folder select view. */
public class ImprovedBookmarkFolderSelectRowCoordinator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkModel mBookmarkModel;
    private final ImprovedBookmarkFolderViewCoordinator mFolderViewCoordinator;

    private View mView;
    private BookmarkId mBookmarkId;
    private BookmarkItem mBookmarkItem;
    private PropertyModelChangeProcessor mChangeProcessor;

    /**
     * @param context The calling context.
     * @param view The view this coordinator controls.
     * @param bookmarkImageFetcher Fetches images for bookmarks.
     * @param bookmarkId The bookmark id to show the row for.
     * @param bookmarkModel The bookmark model used to query bookmark properties.
     * @param clickListener The listener for a row click event.
     */
    public ImprovedBookmarkFolderSelectRowCoordinator(Context context,
            BookmarkImageFetcher bookmarkImageFetcher, BookmarkModel bookmarkModel,
            Runnable clickListener) {
        mContext = context;
        mModel = new PropertyModel(ImprovedBookmarkFolderSelectRowProperties.ALL_KEYS);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkModel = bookmarkModel;
        mFolderViewCoordinator = new ImprovedBookmarkFolderViewCoordinator(
                mContext, mBookmarkImageFetcher, mBookmarkModel);

        mModel.set(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE, true);
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER,
                (v) -> { clickListener.run(); });
    }

    /** Sets the given bookmark id. */
    public void setBookmarkId(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        mBookmarkItem = mBookmarkModel.getBookmarkById(mBookmarkId);

        mModel.set(ImprovedBookmarkFolderSelectRowProperties.TITLE, mBookmarkItem.getTitle());
        mFolderViewCoordinator.setBookmarkId(bookmarkId);
    }

    /** Sets the view that this coordinator controls. */
    public void setView(ImprovedBookmarkFolderSelectRow view) {
        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
        }

        mView = view;
        if (view == null) return;

        mChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mView, ImprovedBookmarkFolderSelectRowViewBinder::bind);
        mFolderViewCoordinator.setView(mView.findViewById(R.id.folder_view));
    }

    /** Returns the {@link View} this coordinator controls. */
    public View getView() {
        return mView;
    }

    /** Returns the {@link PropertyModel}. */
    public PropertyModel getModel() {
        return mModel;
    }

    /** Returns the bookmark id currently assigned to the coordinator. */
    public BookmarkId getBookmarkIdForTesting() {
        return mBookmarkId;
    }
}
