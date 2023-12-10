// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

import org.chromium.android_webview.common.SafeModeController;

import java.util.Set;

/**
 * A {@link ContentProvider} to fetch SafeMode state. No special permissions are required to access
 * this ContentProvider, and it can be accessed by any context (including the embedded WebView
 * implementation).
 */
public final class SafeModeContentProvider extends ContentProvider {
    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        // Not supported
        return 0;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        // Not supported
        return 0;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        // Not supported
        return null;
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        if (SafeModeController.SAFE_MODE_ACTIONS_URI_PATH.equals(uri.getPath())) {
            final String[] columns = {SafeModeController.ACTIONS_COLUMN};
            Set<String> actions = SafeModeService.getSafeModeConfig();
            MatrixCursor cursor = new MatrixCursor(columns, actions.size());
            for (String action : actions) {
                cursor.addRow(new Object[] {action});
            }
            return cursor;
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        // Not supported
        return null;
    }
}
