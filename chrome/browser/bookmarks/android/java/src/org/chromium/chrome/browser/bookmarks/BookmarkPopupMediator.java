// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupMediator {
    private final PropertyModel mPropertyModel;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final Context mContext;
    private final Profile mProfile;
    private final Runnable mDismissRunnable;
    private final CallbackController mCallbackController = new CallbackController();
    private @Nullable BookmarkId mBookmarkId;
    private String mCurrentTitle = "";

    /**
     * Constructor.
     *
     * @param propertyModel The {@link PropertyModel} to populate with bookmark details.
     * @param bookmarkModel The {@link BookmarkModel} to retrieve details and save edits.
     * @param bookmarkManagerOpener Interface to open the bookmark manager.
     * @param bookmarkImageFetcher Fetches icons/images for the bookmark. The mediator takes
     *     ownership of this object and is responsible for destroying it.
     * @param context The Android context.
     * @param profile The current Profile.
     * @param dismissRunnable Runnable to execute when dismissing the popup.
     */
    public BookmarkPopupMediator(
            PropertyModel propertyModel,
            BookmarkModel bookmarkModel,
            BookmarkManagerOpener bookmarkManagerOpener,
            BookmarkImageFetcher bookmarkImageFetcher,
            Context context,
            Profile profile,
            Runnable dismissRunnable) {
        mPropertyModel = propertyModel;
        mBookmarkModel = bookmarkModel;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mContext = context;
        mProfile = profile;
        mDismissRunnable = dismissRunnable;

        mPropertyModel.set(
                BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, this::onRemoveClicked);
        mPropertyModel.set(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, mDismissRunnable);
        mPropertyModel.set(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, this::onDoneClicked);
        mPropertyModel.set(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, this::onTitleChanged);
        mPropertyModel.set(
                BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, this::onFolderRowClicked);
    }

    /** Destroys the mediator, cancelling any pending callbacks. */
    public void destroy() {
        mCallbackController.destroy();
        mBookmarkImageFetcher.destroy();
    }

    /**
     * Shows the bookmark edit popup for the given bookmark.
     *
     * @param bookmarkId The ID of the bookmark to show.
     * @param isNewBookmark Whether this popup is for a newly added bookmark.
     */
    public void show(BookmarkId bookmarkId, boolean isNewBookmark) {
        mBookmarkId = bookmarkId;
        mPropertyModel.set(
                BookmarkPopupProperties.HEADER_TEXT,
                mContext.getString(
                        isNewBookmark ? R.string.bookmark_added : R.string.edit_bookmark));
        mBookmarkModel.finishLoadingBookmarkModel(
                mCallbackController.makeCancelable(
                        () -> {
                            BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
                            if (item != null) {
                                mCurrentTitle = item.getTitle();
                                mPropertyModel.set(BookmarkPopupProperties.TITLE, mCurrentTitle);
                                BookmarkItem parent =
                                        mBookmarkModel.getBookmarkById(item.getParentId());
                                if (parent != null) {
                                    mPropertyModel.set(
                                            BookmarkPopupProperties.FOLDER_NAME, parent.getTitle());
                                }

                                // Pass 0 as the imageSize to fetch the original image/favicon size
                                // without any downscaling constraints, allowing the ImageView to
                                // scale it automatically using its layout bounds.
                                mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                                        item,
                                        /* imageSize= */ 0,
                                        mCallbackController.makeCancelable(
                                                (Drawable drawable) -> {
                                                    mPropertyModel.set(
                                                            BookmarkPopupProperties.IMAGE_DRAWABLE,
                                                            drawable);
                                                }));
                            }
                        }));
    }

    /** Returns the {@link BookmarkId} of the bookmark currently loaded, or null. */
    public @Nullable BookmarkId getBookmarkId() {
        return mBookmarkId;
    }

    private void onTitleChanged(String title) {
        mCurrentTitle = title;
    }

    private void onRemoveClicked() {
        if (mBookmarkId != null) {
            mBookmarkModel.deleteBookmark(mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onFolderRowClicked() {
        if (mBookmarkId != null) {
            mBookmarkManagerOpener.startEditActivity(mContext, mProfile, mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onDoneClicked() {
        if (mBookmarkId != null && mCurrentTitle != null) {
            mBookmarkModel.setBookmarkTitle(mBookmarkId, mCurrentTitle);
        }
        mDismissRunnable.run();
    }
}
