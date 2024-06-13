// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;

import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.Log;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * SafeModeVariationsSeedContentProvider is a content provider that shares the
 * Variations seed with all the WebViews on the system in safe mode situations.
 * A WebView will query the ContentProvider, receiving a file descriptor to which the
 * caller should read the seed contents into its local memory space.
 */
public class SafeModeVariationsSeedContentProvider extends ContentProvider {
    private static final String TAG = "SMVariationSeedCtnt";
    private static final String URI_PATH = VariationsFastFetchModeUtils.URI_PATH;
    private static final long TIMEOUT_IN_MILLIS = 1000;

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
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }

    @Override
    public AssetFileDescriptor openTypedAssetFile(
            Uri uri, String mimeTypeFilter, Bundle opts, CancellationSignal signal) {
        if (!URI_PATH.equals(uri.getPath())) return null;

        // make a best effort to wait on a new SafeMode variations seed
        Boolean success = awaitSeedResults();
        if (!success) {
            Log.d(TAG, "Timeout waiting on seed fetch completion.");
        }

        try {
            ParcelFileDescriptor pfd =
                    ParcelFileDescriptor.open(
                            VariationsUtils.getSeedFile(), ParcelFileDescriptor.MODE_READ_ONLY);
            return new AssetFileDescriptor(pfd, 0, AssetFileDescriptor.UNKNOWN_LENGTH);
        } catch (IOException e) {
            Log.e(TAG, "Failure opening seed file");
        }
        return null;
    }

    private Boolean awaitSeedResults() {
        CountDownLatch countDownLatch = new CountDownLatch(1);
        VariationsSeedHolder.getInstance()
                .hasSeedUpdateCompletedAsync(
                        () -> {
                            countDownLatch.countDown();
                        });
        try {
            return countDownLatch.await(TIMEOUT_IN_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, e.toString());
            return false;
        }
    }

    @Override
    public String getType(Uri uri) {
        throw new UnsupportedOperationException();
    }
}
