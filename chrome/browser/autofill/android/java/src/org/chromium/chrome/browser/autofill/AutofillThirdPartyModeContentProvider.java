// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.support.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A {@link ContentProvider} to fetch Autofill third party mode state. */
@NullMarked
public final class AutofillThirdPartyModeContentProvider extends ContentProvider {
    @VisibleForTesting
    static final String AUTOFILL_THIRD_PARTY_MODE_SHARED_PREFS_FILE =
            "autofill_third_party_mode_shared_prefs_file";

    @VisibleForTesting
    static final String AUTOFILL_THIRD_PARTY_MODE_KEY = "AUTOFILL_THIRD_PARTY_MODE_KEY";

    private static final String AUTOFILL_THIRD_PARTY_MODE_URI_AUTHORITY_SUFFIX =
            ".AutofillThirdPartyModeContentProvider";

    @VisibleForTesting
    static final String AUTOFILL_THIRD_PARTY_MODE_ACTIONS_URI_PATH = "autofill_third_party_mode";

    @VisibleForTesting
    static final String AUTOFILL_THIRD_PARTY_MODE_COLUMN = "autofill_third_party_state";

    @Override
    public boolean onCreate() {
        // Return true to indicate that the provider was successfully loaded.
        return true;
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues values,
            @Nullable String where,
            String @Nullable [] whereArgs) {
        // Not supported
        return 0;
    }

    @Override
    public int delete(Uri uri, @Nullable String selection, String @Nullable [] selectionArgs) {
        // Not supported
        return 0;
    }

    @Override
    public @Nullable Uri insert(Uri uri, @Nullable ContentValues values) {
        // Not supported
        return null;
    }

    @Override
    public @Nullable Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        if (createContentUri().equals(uri)) {
            final String[] columns = {AUTOFILL_THIRD_PARTY_MODE_COLUMN};
            MatrixCursor cursor = new MatrixCursor(columns, 1);
            boolean thirdPartyModeActive =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(
                                    AUTOFILL_THIRD_PARTY_MODE_SHARED_PREFS_FILE,
                                    Context.MODE_PRIVATE)
                            .getBoolean(AUTOFILL_THIRD_PARTY_MODE_KEY, false);
            cursor.addRow(new Object[] {thirdPartyModeActive ? 1 : 0});
            return cursor;
        }
        return null;
    }

    @Override
    public @Nullable String getType(Uri ui) {
        // Not supported
        return null;
    }

    @VisibleForTesting
    static Uri createContentUri() {
        Uri uri =
                new Uri.Builder()
                        .scheme(ContentResolver.SCHEME_CONTENT)
                        .authority(
                                ContextUtils.getApplicationContext().getPackageName()
                                        + AUTOFILL_THIRD_PARTY_MODE_URI_AUTHORITY_SUFFIX)
                        .path(AUTOFILL_THIRD_PARTY_MODE_ACTIONS_URI_PATH)
                        .build();
        return uri;
    }
}
