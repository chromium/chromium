// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class representing the UI state of the {@link BookmarkManager}. All
 * states can be uniquely identified by a URL.
 */
public class BookmarkUiState {
    @IntDef({BookmarkUiMode.INVALID, BookmarkUiMode.LOADING, BookmarkUiMode.FOLDER,
            BookmarkUiMode.SEARCHING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkUiMode {
        int INVALID = 0;
        int LOADING = 1;
        int FOLDER = 2;
        int SEARCHING = 3;
    }
    private static final String SHOPPING_FILTER_URL =
            UrlConstants.BOOKMARKS_FOLDER_URL + "/shopping";

    @BookmarkUiMode
    int mUiMode;
    String mUrl;
    BookmarkId mFolder;

    static BookmarkUiState createLoadingState() {
        BookmarkUiState state = new BookmarkUiState();
        state.mUiMode = BookmarkUiMode.LOADING;
        state.mUrl = "";
        return state;
    }

    static BookmarkUiState createSearchState() {
        BookmarkUiState state = new BookmarkUiState();
        state.mUiMode = BookmarkUiMode.SEARCHING;
        state.mUrl = "";
        return state;
    }

    static BookmarkUiState createShoppingFilterState() {
        BookmarkUiState state = new BookmarkUiState();
        state.mUiMode = BookmarkUiMode.FOLDER;
        state.mUrl = SHOPPING_FILTER_URL;
        state.mFolder = BookmarkId.SHOPPING_FOLDER;
        return state;
    }

    static BookmarkUiState createFolderState(BookmarkId folder, BookmarkModel bookmarkModel) {
        if (BookmarkId.SHOPPING_FOLDER.equals(folder)) return createShoppingFilterState();
        return createStateFromUrl(createFolderUrl(folder), bookmarkModel);
    }

    /**
     * @see #createStateFromUrl(Uri, BookmarkModel)
     */
    static BookmarkUiState createStateFromUrl(String url, BookmarkModel bookmarkModel) {
        if (SHOPPING_FILTER_URL.equals(url)) return createShoppingFilterState();
        return createStateFromUrl(Uri.parse(url), bookmarkModel);
    }

    /**
     * @return A state corresponding to the URI object. If the URI is not valid,
     *         return all_bookmarks.
     */
    static BookmarkUiState createStateFromUrl(Uri uri, BookmarkModel bookmarkModel) {
        BookmarkUiState state = new BookmarkUiState();
        state.mUiMode = BookmarkUiMode.INVALID;
        state.mUrl = uri.toString();

        if (state.mUrl.equals(UrlConstants.BOOKMARKS_URL)) {
            return createFolderState(bookmarkModel.getDefaultFolderViewLocation(), bookmarkModel);
        } else if (state.mUrl.startsWith(UrlConstants.BOOKMARKS_FOLDER_URL)) {
            String path = uri.getLastPathSegment();
            if (!path.isEmpty()) {
                state.mFolder = BookmarkId.getBookmarkIdFromString(path);
                state.mUiMode = BookmarkUiMode.FOLDER;
            }
        }

        if (!state.isValid(bookmarkModel)) {
            state = createFolderState(bookmarkModel.getDefaultFolderViewLocation(), bookmarkModel);
        }

        return state;
    }

    public static Uri createFolderUrl(BookmarkId folderId) {
        Uri.Builder builder = Uri.parse(UrlConstants.BOOKMARKS_FOLDER_URL).buildUpon();
        // Encodes the path and appends it to the base url. A simple appending
        // does not work because there might be spaces in suffix.
        builder.appendPath(folderId.toString());
        return builder.build();
    }

    private BookmarkUiState() {}

    @Override
    public int hashCode() {
        return 31 * mUrl.hashCode() + mUiMode;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof BookmarkUiState)) return false;
        BookmarkUiState other = (BookmarkUiState) obj;
        return mUiMode == other.mUiMode && TextUtils.equals(mUrl, other.mUrl);
    }

    /**
     * @return Whether this state is valid.
     */
    boolean isValid(BookmarkModel bookmarkModel) {
        if (mUrl == null || mUiMode == BookmarkUiMode.INVALID) return false;
        if (mUrl.equals(SHOPPING_FILTER_URL)) return true;

        if (mUiMode == BookmarkUiMode.FOLDER) {
            return mFolder != null && bookmarkModel.doesBookmarkExist(mFolder);
        }

        return true;
    }
}
