// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.PrintWriter;

/**
 * ContentProvider base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
@NullMarked
public class SplitCompatContentProvider extends ContentProvider {
    private final Object mImplLock = new Object();
    private @Nullable Impl mImpl;
    private final String mContentProviderClassName;

    public SplitCompatContentProvider(String contentProviderClassName) {
        mContentProviderClassName = contentProviderClassName;
    }

    private Impl getImpl() {
        // Content provider methods can be called on multiple threads, so make sure mImpl is locked
        // when it is created.
        synchronized (mImplLock) {
            if (mImpl == null) {
                mImpl =
                        (Impl)
                                BundleUtils.newInstance(
                                        mContentProviderClassName, CHROME_SPLIT_NAME);
                mImpl.setContentProvider(this);
            }
            return mImpl;
        }
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public @Nullable Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        return getImpl().query(uri, projection, selection, selectionArgs, sortOrder);
    }

    @Override
    public @Nullable Uri insert(Uri uri, @Nullable ContentValues values) {
        return getImpl().insert(uri, values);
    }

    @Override
    public int delete(Uri uri, @Nullable String selection, String @Nullable [] selectionArgs) {
        return getImpl().delete(uri, selection, selectionArgs);
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues values,
            @Nullable String selection,
            String @Nullable [] selectionArgs) {
        return getImpl().update(uri, values, selection, selectionArgs);
    }

    @Override
    public @Nullable String getType(Uri uri) {
        return getImpl().getType(uri);
    }

    @Override
    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        getImpl().dump(fd, writer, args);
    }

    @Override
    public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
            throws FileNotFoundException {
        return getImpl().openFile(uri, mode);
    }

    /**
     * Holds the implementation of ContentProvider logic. Will be called by {@link
     * SplitCompatContentProvider}.
     */
    public abstract static class Impl {
        private SplitCompatContentProvider mContentProvider;

        @Initializer
        protected void setContentProvider(SplitCompatContentProvider contentProvider) {
            mContentProvider = contentProvider;
        }

        protected final @Nullable Context getContext() {
            return mContentProvider.getContext();
        }

        protected final @Nullable String getCallingPackage() {
            return mContentProvider.getCallingPackage();
        }

        public abstract @Nullable Cursor query(
                Uri uri,
                String @Nullable [] projection,
                @Nullable String selection,
                String @Nullable [] selectionArgs,
                @Nullable String sortOrder);

        public abstract @Nullable Uri insert(Uri uri, @Nullable ContentValues values);

        public abstract int delete(
                Uri uri, @Nullable String selection, String @Nullable [] selectionArgs);

        public abstract int update(
                Uri uri,
                @Nullable ContentValues values,
                @Nullable String selection,
                String @Nullable [] selectionArgs);

        public abstract @Nullable String getType(Uri uri);

        public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {}

        public ParcelFileDescriptor openFile(@NonNull Uri uri, @NonNull String mode)
                throws FileNotFoundException {
            throw new FileNotFoundException();
        }
    }
}
