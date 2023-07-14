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
    private final View mView;
    private final PropertyModel mModel;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkId mBookmarkId;
    private final BookmarkItem mBookmarkItem;
    private final BookmarkModel mBookmarkModel;
    private final ImprovedBookmarkFolderViewCoordinator mFolderViewCoordinator;

    /**
     * @param context The calling context.
     * @param view The view this coordinator controls.
     * @param bookmarkImageFetcher Fetches images for bookmarks.
     * @param bookmarkId The folder to show the row for.
     * @param bookmarkModel The bookmark model used to query bookmark properties.
     */
    public ImprovedBookmarkFolderSelectRowCoordinator(Context context,
            ImprovedBookmarkFolderSelectRow view, BookmarkImageFetcher bookmarkImageFetcher,
            BookmarkId bookmarkId, BookmarkModel bookmarkModel) {
        mContext = context;
        mView = view;
        mModel = new PropertyModel(ImprovedBookmarkFolderSelectRowProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mView, ImprovedBookmarkFolderSelectRowViewBinder::bind);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkId = bookmarkId;
        mBookmarkModel = bookmarkModel;
        mBookmarkItem = mBookmarkModel.getBookmarkById(mBookmarkId);
        mFolderViewCoordinator = new ImprovedBookmarkFolderViewCoordinator(
                mContext, mBookmarkImageFetcher, mBookmarkId, mBookmarkModel);
        mFolderViewCoordinator.setView(mView.findViewById(R.id.folder_view));

        mModel.set(ImprovedBookmarkFolderSelectRowProperties.TITLE,
                mBookmarkModel.getBookmarkTitle(mBookmarkId));
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE, true);
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER,
                (v)
                        -> {
                                // TODO(crbug.com/1448933): Implement new move activity.
                        });
    }

    public PropertyModel getModel() {
        return mModel;
    }

    public View getView() {
        return mView;
    }
}
