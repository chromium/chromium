// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Package path can be changed, but should match <manifest package="..."> in AndroidManifest.xml.
package org.chromium.example.partnercustomizations;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

/**
 * A sample ContentProvider demonstrating the various partner configurations: adding a homepage,
 * disabling incognito mode, and adding default bookmarks.
 *
 * This class name can be changed, but should match <provider android:name="..."> in
 * AndroidManifest.xml.
 */
public class PartnerBrowserCustomizationsProviderExample extends ContentProvider {
    // "http://www.android.com/" is just an example. Please replace this to actual homepage.
    // Other strings in this class must remain as it is.
    private static final String HOMEPAGE_URI = "http://www.android.com/";
    private static final int URI_MATCH_HOMEPAGE = 0;
    private static final int URI_MATCH_DISABLE_INCOGNITO_MODE = 1;
    private static final int URI_MATCH_DISABLE_BOOKMARKS_EDITING = 2;
    private static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);
    static {
        URI_MATCHER.addURI(
                "com.android.partnerbrowsercustomizations", "homepage", URI_MATCH_HOMEPAGE);
        URI_MATCHER.addURI("com.android.partnerbrowsercustomizations", "disableincognitomode",
                URI_MATCH_DISABLE_INCOGNITO_MODE);
        URI_MATCHER.addURI("com.android.partnerbrowsercustomizations", "disablebookmarksediting",
                URI_MATCH_DISABLE_BOOKMARKS_EDITING);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public String getType(Uri uri) {
        // In fact, Chrome does not call this.
        // Just a recommaned ContentProvider practice in general.
        switch (URI_MATCHER.match(uri)) {
            case URI_MATCH_HOMEPAGE:
                return "vnd.android.cursor.item/partnerhomepage";
            case URI_MATCH_DISABLE_INCOGNITO_MODE:
                return "vnd.android.cursor.item/partnerdisableincognitomode";
            case URI_MATCH_DISABLE_BOOKMARKS_EDITING:
                return "vnd.android.cursor.item/partnerdisablebookmarksediting";
            default:
                return null;
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        switch (URI_MATCHER.match(uri)) {
            case URI_MATCH_HOMEPAGE: {
                MatrixCursor cursor = new MatrixCursor(new String[] {"homepage"}, 1);
                cursor.addRow(new Object[] {HOMEPAGE_URI});
                return cursor;
            }
            case URI_MATCH_DISABLE_INCOGNITO_MODE: {
                MatrixCursor cursor = new MatrixCursor(new String[] {"disableincognitomode"}, 1);
                cursor.addRow(new Object[] {1});
                return cursor;
            }
            case URI_MATCH_DISABLE_BOOKMARKS_EDITING: {
                MatrixCursor cursor = new MatrixCursor(new String[] {"disablebookmarksediting"}, 1);
                cursor.addRow(new Object[] {1});
                return cursor;
            }
            default:
                return null;
        }
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }
}
