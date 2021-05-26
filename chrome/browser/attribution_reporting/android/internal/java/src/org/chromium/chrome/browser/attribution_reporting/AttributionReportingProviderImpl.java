// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A ContentProvider used to push Attribution data into Chrome from other Apps.
 * This ContentProvider is append-only and does not expose any update or query capabilities.
 */
public class AttributionReportingProviderImpl extends AttributionReportingProvider.Impl {
    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri unused, ContentValues values) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)) {
            return null;
        }
        if (!values.containsKey(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID)
                || !values.containsKey(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION)) {
            throw new IllegalArgumentException("Missing attribution key(s).");
        }

        // TODO(https://crbug.com/1210171): Handle the attribution data with job scheduler if Chrome
        // is not started, and a callback if it is.

        // We don't have a meaningful Uri to return, so just return an empty one to indicate success
        // (in place of null for failure).
        return Uri.EMPTY;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }
}
