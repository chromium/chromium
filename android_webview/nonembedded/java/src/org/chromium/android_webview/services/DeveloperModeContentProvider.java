// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

import org.chromium.android_webview.common.DeveloperModeUtils;

import java.util.Map;

/**
 * A {@link ContentProvider} to fetch debugging data via the {@code query()} method. No special
 * permissions are required to access this ContentProvider, and it can be accessed by any context
 * (including the embedded WebView implementation).
 */
public final class DeveloperModeContentProvider extends ContentProvider {
    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        if (DeveloperModeUtils.FLAG_OVERRIDE_URI_PATH.equals(uri.getPath())) {
            Map<String, Boolean> flagOverrides = DeveloperUiService.getFlagOverrides();
            final String[] columns = {DeveloperModeUtils.FLAG_OVERRIDE_NAME_COLUMN,
                    DeveloperModeUtils.FLAG_OVERRIDE_STATE_COLUMN};
            MatrixCursor cursor = new MatrixCursor(columns, flagOverrides.size());
            for (Map.Entry<String, Boolean> entry : flagOverrides.entrySet()) {
                String flagName = entry.getKey();
                boolean enabled = entry.getValue();
                cursor.addRow(new Object[] {flagName, enabled ? 1 : 0});
            }
            return cursor;
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        throw new UnsupportedOperationException();
    }
}
