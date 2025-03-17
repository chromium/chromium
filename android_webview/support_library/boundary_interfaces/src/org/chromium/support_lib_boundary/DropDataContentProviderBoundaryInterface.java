// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.ContentProvider;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.io.FileNotFoundException;

/** Boundary interface for DropDataProvider. */
@NullMarked
public interface DropDataContentProviderBoundaryInterface {
    boolean onCreate();

    String @Nullable [] getStreamTypes(Uri uri, String mimeTypeFilter);

    @Nullable ParcelFileDescriptor openFile(ContentProvider providerWrapper, Uri uri)
            throws FileNotFoundException;

    Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder);

    @Nullable String getType(@Nullable Uri uri);

    Uri cache(byte[] imageBytes, String encodingFormat, String filename);

    void setClearCachedDataIntervalMs(int milliseconds);

    void onDragEnd(boolean imageInUse);

    @Nullable Bundle call(String method, @Nullable String arg, @Nullable Bundle extras);
}
