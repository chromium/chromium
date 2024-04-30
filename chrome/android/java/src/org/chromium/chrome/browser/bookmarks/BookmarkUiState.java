// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** A class representing the UI state of the {@link BookmarkManagerMediator}. */
public class BookmarkUiState {
    @IntDef({
        BookmarkUiMode.INVALID,
        BookmarkUiMode.LOADING,
        BookmarkUiMode.FOLDER,
        BookmarkUiMode.SEARCHING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkUiMode {
        int INVALID = 0;
        int LOADING = 1;
        int FOLDER = 2;
        int SEARCHING = 3;
    }

    final @BookmarkUiMode int mUiMode;
    final @NonNull String mUrl;
    final @Nullable BookmarkId mFolder;

    // The following fields be non-null if and only if in SEARCHING mode.
    final @Nullable String mSearchText;

    static BookmarkUiState createLoadingState() {
        return new BookmarkUiState(
                BookmarkUiMode.LOADING, /* url= */ "", /* folder= */ null, /* queryString= */ null);
    }

    static BookmarkUiState createSearchState(@NonNull String queryString) {
        return new BookmarkUiState(
                BookmarkUiMode.SEARCHING, /* url= */ "", /* folder= */ null, queryString);
    }

    static BookmarkUiState createFolderState(BookmarkId folder, BookmarkModel bookmarkModel) {
        return createStateFromUrl(createFolderUrl(folder), bookmarkModel);
    }

    /**
     * @see #createStateFromUrl(Uri, BookmarkModel).
     */
    public static BookmarkUiState createStateFromUrl(String url, BookmarkModel bookmarkModel) {
        return createStateFromUrl(Uri.parse(url), bookmarkModel);
    }

    /**
     * @return A state corresponding to the URI object. If the URI is not valid a folder state for
     *     the root folder will be returned.
     */
    static BookmarkUiState createStateFromUrl(Uri uri, BookmarkModel bookmarkModel) {
        String url = uri.toString();

        BookmarkUiState tempState = null;
        if (url.equals(UrlConstants.BOOKMARKS_URL)) {
            return createFolderState(bookmarkModel.getDefaultFolderViewLocation(), bookmarkModel);
        } else if (url.startsWith(UrlConstants.BOOKMARKS_FOLDER_URL)) {
            String path = uri.getLastPathSegment();
            if (!path.isEmpty()) {
                tempState =
                        new BookmarkUiState(
                                BookmarkUiMode.FOLDER,
                                url,
                                BookmarkId.getBookmarkIdFromString(path),
                                /* queryString= */ null);
            }
        }

        if (tempState != null && tempState.isValid(bookmarkModel)) {
            return tempState;
        } else {
            return createFolderState(bookmarkModel.getDefaultFolderViewLocation(), bookmarkModel);
        }
    }

    public static Uri createFolderUrl(BookmarkId folderId) {
        Uri.Builder builder = Uri.parse(UrlConstants.BOOKMARKS_FOLDER_URL).buildUpon();
        // Encodes the path and appends it to the base url. A simple appending
        // does not work because there might be spaces in suffix.
        builder.appendPath(folderId.toString());
        return builder.build();
    }

    private BookmarkUiState(
            @BookmarkUiMode int uiMode,
            @NonNull String url,
            BookmarkId folder,
            @Nullable String queryString) {
        assert (uiMode == BookmarkUiMode.SEARCHING) != (queryString == null);
        mUiMode = uiMode;
        mUrl = url;
        mFolder = folder;
        mSearchText = queryString;
    }

    public @Nullable BookmarkId getFolder() {
        return mFolder;
    }

    @Override
    public int hashCode() {
        return 31 * mUrl.hashCode() + mUiMode + Objects.hashCode(mSearchText);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof BookmarkUiState)) return false;
        BookmarkUiState other = (BookmarkUiState) obj;
        return mUiMode == other.mUiMode
                && TextUtils.equals(mUrl, other.mUrl)
                && Objects.equals(mSearchText, other.mSearchText);
    }

    /** Returns whether this state is valid. */
    boolean isValid(BookmarkModel bookmarkModel) {
        if (mUrl == null || mUiMode == BookmarkUiMode.INVALID) return false;

        if (mUiMode == BookmarkUiMode.FOLDER) {
            return mFolder != null && bookmarkModel.doesBookmarkExist(mFolder);
        }

        return true;
    }
}
