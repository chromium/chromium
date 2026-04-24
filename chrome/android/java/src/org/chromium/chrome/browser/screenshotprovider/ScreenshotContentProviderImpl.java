// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;

import java.io.FileNotFoundException;

/** ContentProvider that serves screenshots. */
@UsedByReflection("ScreenshotContentProvider.java")
@NullMarked
public class ScreenshotContentProviderImpl extends SplitCompatContentProvider.Impl {

    @Override
    public @Nullable Cursor query(
            Uri uri,
            String @Nullable [] strings,
            @Nullable String s,
            String @Nullable [] strings1,
            @Nullable String s1) {
        return null;
    }

    @Override
    public @Nullable String getType(Uri uri) {
        return null;
    }

    @Override
    public @Nullable Uri insert(Uri uri, @Nullable ContentValues contentValues) {
        return null;
    }

    @Override
    public int delete(Uri uri, @Nullable String s, String @Nullable [] strings) {
        return 0;
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues contentValues,
            @Nullable String s,
            String @Nullable [] strings) {
        return 0;
    }

    @Override
    public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
            throws FileNotFoundException {
        throw new FileNotFoundException("No implementation");
    }
}
