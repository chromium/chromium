// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.FutureTask;

/**
 * A ContentProvider used to push Attribution data into Chrome from other Apps.
 * This ContentProvider is append-only and does not expose any update or query capabilities.
 */
public class AttributionReportingProviderImpl extends AttributionReportingProvider.Impl {
    private static final String TAG = "AttributionReporting";

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

        final String sourceEventId =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID);
        final String destination =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION);

        if (TextUtils.isEmpty(sourceEventId) || TextUtils.isEmpty(destination)) {
            throw new IllegalArgumentException("Missing attribution key(s).");
        }

        final String sourcePackageName = getCallingPackage();
        final String reportTo =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO);
        final Long expiry = values.getAsLong(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY);

        FutureTask<Uri> insertTask = new FutureTask<>(() -> {
            // TODO(https://crbug.com/1210171): Handle the attribution data with job scheduler if
            // Chrome is not started.
            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                return Uri.EMPTY;
            }

            return insertOnUiThread(
                    values, sourceEventId, destination, sourcePackageName, reportTo, expiry);
        });

        // This cannot be BEST_EFFORT as the browser may not yet be started and so BEST_EFFORT tasks
        // will not be run.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, insertTask);
        try {
            return insertTask.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private Uri insertOnUiThread(final ContentValues values, final String sourceEventId,
            final String destination, final String sourcePackageName, final String reportTo,
            final Long expiry) {
        AttributionReporter.getInstance().reportAppImpression(Profile.getLastUsedRegularProfile(),
                sourcePackageName, sourceEventId, destination, reportTo,
                expiry == null ? 0 : expiry.longValue());

        // We don't have a meaningful Uri to return, so just return an empty one to indicate
        // success (in place of null for failure).
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
