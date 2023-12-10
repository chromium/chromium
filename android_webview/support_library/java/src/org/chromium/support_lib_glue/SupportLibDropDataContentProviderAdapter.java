// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.ContentProvider;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.support_lib_boundary.DropDataContentProviderBoundaryInterface;
import org.chromium.ui.dragdrop.DropDataProviderImpl;

import java.io.FileNotFoundException;

public class SupportLibDropDataContentProviderAdapter
        implements DropDataContentProviderBoundaryInterface {
    private DropDataProviderImpl mProviderImpl;

    @Override
    public boolean onCreate() {
        mProviderImpl = DropDataProviderImpl.onCreate();
        return true;
    }

    @Override
    public String[] getStreamTypes(@NonNull Uri uri, @NonNull String mimeTypeFilter) {
        return mProviderImpl.getStreamTypes(uri, mimeTypeFilter);
    }

    @Override
    public ParcelFileDescriptor openFile(@NonNull ContentProvider providerWrapper, @NonNull Uri uri)
            throws FileNotFoundException {
        return mProviderImpl.openFile(providerWrapper, uri);
    }

    @Override
    public Cursor query(
            @NonNull Uri uri,
            @Nullable String[] projection,
            @Nullable String selection,
            @Nullable String[] selectionArgs,
            @Nullable String sortOrder) {
        return mProviderImpl.query(uri, projection);
    }

    @Override
    public String getType(@NonNull Uri uri) {
        return mProviderImpl.getType(uri);
    }

    @Override
    public Uri cache(byte[] imageBytes, String encodingFormat, String filename) {
        return mProviderImpl.cache(imageBytes, encodingFormat, filename);
    }

    @Override
    public void setClearCachedDataIntervalMs(int milliseconds) {
        mProviderImpl.setClearCachedDataIntervalMs(milliseconds);
    }

    @Override
    public void onDragEnd(boolean imageInUse) {
        mProviderImpl.onDragEnd(imageInUse);
    }

    @Override
    public Bundle call(@NonNull String method, @Nullable String arg, @Nullable Bundle extras) {
        return mProviderImpl.call(method, arg, extras);
    }
}
