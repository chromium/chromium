// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.ContentProvider;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.FileNotFoundException;

/** Boundary interface for DropDataProvider. */
public interface DropDataContentProviderBoundaryInterface {
    boolean onCreate();

    String[] getStreamTypes(@NonNull Uri uri, @NonNull String mimeTypeFilter);

    ParcelFileDescriptor openFile(@NonNull ContentProvider providerWrapper, @NonNull Uri uri)
            throws FileNotFoundException;

    Cursor query(
            @NonNull Uri uri,
            @Nullable String[] projection,
            @Nullable String selection,
            @Nullable String[] selectionArgs,
            @Nullable String sortOrder);

    String getType(@NonNull Uri uri);

    Uri cache(byte[] imageBytes, String encodingFormat, String filename);

    void setClearCachedDataIntervalMs(int milliseconds);

    void onDragEnd(boolean imageInUse);

    Bundle call(@NonNull String method, @Nullable String arg, @Nullable Bundle extras);
}
