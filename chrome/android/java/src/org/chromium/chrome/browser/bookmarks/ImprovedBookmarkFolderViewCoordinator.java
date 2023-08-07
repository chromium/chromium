// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.Pair;
import android.view.View;

import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Business logic for the improved bookmark folder view. */
public class ImprovedBookmarkFolderViewCoordinator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkModel mBookmarkModel;

    private View mView;
    private BookmarkId mBookmarkId;
    private BookmarkItem mBookmarkItem;
    private PropertyModelChangeProcessor mChangeProcessor;

    /**
     * @param context The calling context.
     * @param bookmarkImageFetcher Fetches images for bookmarks.
     * @param bookmarkModel The bookmark model used to query bookmark properties.
     */
    public ImprovedBookmarkFolderViewCoordinator(Context context,
            BookmarkImageFetcher bookmarkImageFetcher, BookmarkModel bookmarkModel) {
        mContext = context;
        mModel = new PropertyModel(ImprovedBookmarkFolderViewProperties.ALL_KEYS);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkModel = bookmarkModel;
    }

    /** Sets the {@link BookmarkId} for the folder view. */
    public void setBookmarkId(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        mBookmarkItem = mBookmarkModel.getBookmarkById(mBookmarkId);

        final @BookmarkType int type = mBookmarkId.getType();
        mModel.set(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR,
                BookmarkUtils.getIconBackground(mContext, mBookmarkModel, mBookmarkItem));
        mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_TINT,
                BookmarkUtils.getIconTint(mContext, mBookmarkModel, mBookmarkItem));
        mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE,
                BookmarkUtils.getFolderIcon(mContext, type, BookmarkRowDisplayPref.VISUAL));
        mModel.set(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT,
                BookmarkUtils.getChildCountForDisplay(mBookmarkId, mBookmarkModel));
        mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                new Pair<>(null, null));
        if (BookmarkUtils.shouldShowImagesForFolder(mBookmarkModel, mBookmarkId)) {
            mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(mBookmarkItem, (imagePair) -> {
                mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                        imagePair);
            });
        }
    }

    /**
     * Sets the {@link View} that this coordinator sets up. Will destroy any previously bound view.
     */
    public void setView(ImprovedBookmarkFolderView view) {
        if (mView == view) return;

        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
        }

        mView = view;
        if (mView == null) return;

        mChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mView, ImprovedBookmarkFolderViewBinder::bind);
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    View getViewForTesting() {
        return mView;
    }
}
