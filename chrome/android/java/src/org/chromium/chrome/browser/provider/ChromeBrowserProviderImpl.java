// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.app.SearchManager;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

/**
 * This class provides access to user data stored in Chrome, such as bookmarks, most visited pages,
 * etc. It is used to support android.provider.Browser.
 */
public class ChromeBrowserProviderImpl extends ChromeBrowserProvider.Impl {
    private static final String TAG = "ChromeBrowserProvider";

    /**
     * A projection of {@link #SEARCHES_URI} that contains {@link SearchColumns#ID},
     * {@link SearchColumns#SEARCH}, and {@link SearchColumns#DATE}.
     */
    @VisibleForTesting
    public static final String[] SEARCHES_PROJECTION =
            new String[] {
                // if you change column order you must also change indices below
                SearchColumns.ID, // 0
                SearchColumns.SEARCH, // 1
                SearchColumns.DATE, // 2
            };

    // Defines Chrome's API authority, so it can be run and tested
    // independently.
    private static final String API_AUTHORITY_SUFFIX = ".browser";

    private static final String BROWSER_CONTRACT_API_AUTHORITY =
            "com.google.android.apps.chrome.browser-contract";

    // These values are taken from android.provider.BrowserContract.java since
    // that class is hidden from the SDK.
    private static final String BROWSER_CONTRACT_AUTHORITY = "com.android.browser";
    private static final String BROWSER_CONTRACT_HISTORY_CONTENT_TYPE =
            "vnd.android.cursor.dir/browser-history";
    private static final String BROWSER_CONTRACT_HISTORY_CONTENT_ITEM_TYPE =
            "vnd.android.cursor.item/browser-history";
    private static final String BROWSER_CONTRACT_BOOKMARK_CONTENT_TYPE =
            "vnd.android.cursor.dir/bookmark";
    private static final String BROWSER_CONTRACT_BOOKMARK_CONTENT_ITEM_TYPE =
            "vnd.android.cursor.item/bookmark";
    private static final String BROWSER_CONTRACT_SEARCH_CONTENT_TYPE =
            "vnd.android.cursor.dir/searches";
    private static final String BROWSER_CONTRACT_SEARCH_CONTENT_ITEM_TYPE =
            "vnd.android.cursor.item/searches";

    // This Authority is for internal interface. It's concatenated with
    // Context.getPackageName() so that we can install different channels
    // SxS and have different authorities.
    private static final String AUTHORITY_SUFFIX = ".ChromeBrowserProvider";
    private static final String BOOKMARKS_PATH = "bookmarks";
    private static final String SEARCHES_PATH = "searches";
    private static final String HISTORY_PATH = "history";
    private static final String COMBINED_PATH = "combined";

    private static final int URI_MATCH_BOOKMARKS = 0;
    private static final int URI_MATCH_BOOKMARKS_ID = 1;
    private static final int URL_MATCH_API_BOOKMARK = 2;
    private static final int URL_MATCH_API_BOOKMARK_ID = 3;
    private static final int URL_MATCH_API_SEARCHES = 4;
    private static final int URL_MATCH_API_SEARCHES_ID = 5;
    private static final int URL_MATCH_API_HISTORY_CONTENT = 6;
    private static final int URL_MATCH_API_HISTORY_CONTENT_ID = 7;
    private static final int URL_MATCH_API_BOOKMARK_CONTENT = 8;
    private static final int URL_MATCH_API_BOOKMARK_CONTENT_ID = 9;
    private static final int URL_MATCH_BOOKMARK_SUGGESTIONS_ID = 10;
    private static final int URL_MATCH_BOOKMARK_HISTORY_SUGGESTIONS_ID = 11;

    private static final String[] BOOKMARK_DEFAULT_PROJECTION =
            new String[] {
                BookmarkColumns.ID,
                BookmarkColumns.URL,
                BookmarkColumns.VISITS,
                BookmarkColumns.DATE,
                BookmarkColumns.BOOKMARK,
                BookmarkColumns.TITLE,
                BookmarkColumns.FAVICON,
                BookmarkColumns.CREATED
            };

    private final Object mInitializeUriMatcherLock = new Object();
    private UriMatcher mUriMatcher;

    private void ensureUriMatcherInitialized() {
        synchronized (mInitializeUriMatcherLock) {
            if (mUriMatcher != null) return;

            mUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
            // The internal URIs
            String authority = getContext().getPackageName() + AUTHORITY_SUFFIX;
            mUriMatcher.addURI(authority, BOOKMARKS_PATH, URI_MATCH_BOOKMARKS);
            mUriMatcher.addURI(authority, BOOKMARKS_PATH + "/#", URI_MATCH_BOOKMARKS_ID);
            // The internal authority for public APIs
            String apiAuthority = getContext().getPackageName() + API_AUTHORITY_SUFFIX;
            mUriMatcher.addURI(apiAuthority, BOOKMARKS_PATH, URL_MATCH_API_BOOKMARK);
            mUriMatcher.addURI(apiAuthority, BOOKMARKS_PATH + "/#", URL_MATCH_API_BOOKMARK_ID);
            mUriMatcher.addURI(apiAuthority, SEARCHES_PATH, URL_MATCH_API_SEARCHES);
            mUriMatcher.addURI(apiAuthority, SEARCHES_PATH + "/#", URL_MATCH_API_SEARCHES_ID);
            mUriMatcher.addURI(apiAuthority, HISTORY_PATH, URL_MATCH_API_HISTORY_CONTENT);
            mUriMatcher.addURI(apiAuthority, HISTORY_PATH + "/#", URL_MATCH_API_HISTORY_CONTENT_ID);
            mUriMatcher.addURI(apiAuthority, COMBINED_PATH, URL_MATCH_API_BOOKMARK);
            mUriMatcher.addURI(apiAuthority, COMBINED_PATH + "/#", URL_MATCH_API_BOOKMARK_ID);
            // The internal authority for BrowserContracts
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY, HISTORY_PATH, URL_MATCH_API_HISTORY_CONTENT);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY,
                    HISTORY_PATH + "/#",
                    URL_MATCH_API_HISTORY_CONTENT_ID);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY, COMBINED_PATH, URL_MATCH_API_BOOKMARK);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY,
                    COMBINED_PATH + "/#",
                    URL_MATCH_API_BOOKMARK_ID);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY, SEARCHES_PATH, URL_MATCH_API_SEARCHES);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY,
                    SEARCHES_PATH + "/#",
                    URL_MATCH_API_SEARCHES_ID);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY, BOOKMARKS_PATH, URL_MATCH_API_BOOKMARK_CONTENT);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_API_AUTHORITY,
                    BOOKMARKS_PATH + "/#",
                    URL_MATCH_API_BOOKMARK_CONTENT_ID);
            // Added the Android Framework URIs, so the provider can easily switched
            // by adding 'browser' and 'com.android.browser' in manifest.
            // The Android's BrowserContract
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_AUTHORITY, HISTORY_PATH, URL_MATCH_API_HISTORY_CONTENT);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_AUTHORITY,
                    HISTORY_PATH + "/#",
                    URL_MATCH_API_HISTORY_CONTENT_ID);
            mUriMatcher.addURI(BROWSER_CONTRACT_AUTHORITY, "combined", URL_MATCH_API_BOOKMARK);
            mUriMatcher.addURI(BROWSER_CONTRACT_AUTHORITY, "combined/#", URL_MATCH_API_BOOKMARK_ID);
            mUriMatcher.addURI(BROWSER_CONTRACT_AUTHORITY, SEARCHES_PATH, URL_MATCH_API_SEARCHES);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_AUTHORITY, SEARCHES_PATH + "/#", URL_MATCH_API_SEARCHES_ID);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_AUTHORITY, BOOKMARKS_PATH, URL_MATCH_API_BOOKMARK_CONTENT);
            mUriMatcher.addURI(
                    BROWSER_CONTRACT_AUTHORITY,
                    BOOKMARKS_PATH + "/#",
                    URL_MATCH_API_BOOKMARK_CONTENT_ID);
            // For supporting android.provider.browser.BookmarkColumns and
            // SearchColumns
            mUriMatcher.addURI("browser", BOOKMARKS_PATH, URL_MATCH_API_BOOKMARK);
            mUriMatcher.addURI("browser", BOOKMARKS_PATH + "/#", URL_MATCH_API_BOOKMARK_ID);
            mUriMatcher.addURI("browser", SEARCHES_PATH, URL_MATCH_API_SEARCHES);
            mUriMatcher.addURI("browser", SEARCHES_PATH + "/#", URL_MATCH_API_SEARCHES_ID);

            mUriMatcher.addURI(
                    apiAuthority,
                    BOOKMARKS_PATH + "/" + SearchManager.SUGGEST_URI_PATH_QUERY,
                    URL_MATCH_BOOKMARK_SUGGESTIONS_ID);
            mUriMatcher.addURI(
                    apiAuthority,
                    SearchManager.SUGGEST_URI_PATH_QUERY,
                    URL_MATCH_BOOKMARK_HISTORY_SUGGESTIONS_ID);
        }
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        return new MatrixCursor(BOOKMARK_DEFAULT_PROJECTION, 0);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }

    @VisibleForTesting
    public static String getApiAuthority(Context context) {
        return context.getPackageName() + API_AUTHORITY_SUFFIX;
    }

    @VisibleForTesting
    public static Uri getBookmarksApiUri(Context context) {
        return buildContentUri(getApiAuthority(context), BOOKMARKS_PATH);
    }

    @VisibleForTesting
    public static Uri getSearchesApiUri(Context context) {
        return buildContentUri(getApiAuthority(context), SEARCHES_PATH);
    }

    @Override
    public String getType(Uri uri) {
        // Keep returning non-null values just in case, to avoid breaking old apps.
        ensureUriMatcherInitialized();
        int match = mUriMatcher.match(uri);
        return switch (match) {
            case URI_MATCH_BOOKMARKS,
                    URL_MATCH_API_BOOKMARK -> BROWSER_CONTRACT_BOOKMARK_CONTENT_TYPE;
            case URI_MATCH_BOOKMARKS_ID,
                    URL_MATCH_API_BOOKMARK_ID -> BROWSER_CONTRACT_BOOKMARK_CONTENT_ITEM_TYPE;
            case URL_MATCH_API_SEARCHES -> BROWSER_CONTRACT_SEARCH_CONTENT_TYPE;
            case URL_MATCH_API_SEARCHES_ID -> BROWSER_CONTRACT_SEARCH_CONTENT_ITEM_TYPE;
            case URL_MATCH_API_HISTORY_CONTENT -> BROWSER_CONTRACT_HISTORY_CONTENT_TYPE;
            case URL_MATCH_API_HISTORY_CONTENT_ID -> BROWSER_CONTRACT_HISTORY_CONTENT_ITEM_TYPE;
            default -> throw new IllegalArgumentException(uri.toString());
        };
    }

    private static Uri buildContentUri(String authority, String path) {
        return Uri.parse("content://" + authority + "/" + path);
    }
}
