// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.Pair;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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

        int type = mBookmarkId.getType();
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_ICON_DRAWABLE,
                BookmarkUtils.getFolderIcon(mContext, type, BookmarkRowDisplayPref.VISUAL));
        if (type == BookmarkType.READING_LIST) {
            mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_AREA_BACKGROUND_COLOR,
                    SemanticColorUtils.getColorPrimaryContainer(mContext));
            mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_ICON_TINT,
                    ColorStateList.valueOf(
                            SemanticColorUtils.getDefaultIconColorAccent1(mContext)));
        } else {
            mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_AREA_BACKGROUND_COLOR,
                    ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_1));
            mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_ICON_TINT,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_secondary_tint_list));
        }

        mModel.set(ImprovedBookmarkFolderSelectRowProperties.TITLE,
                mBookmarkModel.getBookmarkTitle(mBookmarkId));
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.FOLDER_CHILD_COUNT,
                mBookmarkModel.getChildCount(mBookmarkId));
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE, true);
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER,
                (v)
                        -> {
                                // TODO(crbug.com/1448933): Implement new move activity.
                        });
        mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES,
                new Pair<>(null, null));
        if (BookmarkUtils.shouldShowImagesForFolder(mBookmarkModel, mBookmarkId)) {
            mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(mBookmarkItem, (imagePair) -> {
                mModel.set(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES,
                        imagePair);
            });
        }
    }

    public PropertyModel getModel() {
        return mModel;
    }

    public View getView() {
        return mView;
    }
}
