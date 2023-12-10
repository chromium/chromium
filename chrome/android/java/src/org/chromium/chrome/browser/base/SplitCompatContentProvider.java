// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

import org.chromium.base.BundleUtils;

import java.io.FileDescriptor;
import java.io.PrintWriter;

/**
 * ContentProvider base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatContentProvider extends ContentProvider {
    private final Object mImplLock = new Object();
    private Impl mImpl;
    private String mContentProviderClassName;

    public SplitCompatContentProvider(String contentProviderClassName) {
        mContentProviderClassName = contentProviderClassName;
    }

    private Impl getImpl() {
        // Content provider methods can be called on multiple threads, so make sure mImpl is locked
        // when it is created.
        synchronized (mImplLock) {
            if (mImpl == null) {
                Context context = SplitCompatApplication.createChromeContext(getContext());
                mImpl = (Impl) BundleUtils.newInstance(context, mContentProviderClassName);
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
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        return getImpl().query(uri, projection, selection, selectionArgs, sortOrder);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return getImpl().insert(uri, values);
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return getImpl().delete(uri, selection, selectionArgs);
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return getImpl().update(uri, values, selection, selectionArgs);
    }

    @Override
    public String getType(Uri uri) {
        return getImpl().getType(uri);
    }

    @Override
    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        getImpl().dump(fd, writer, args);
    }

    /**
     * Holds the implementation of ContentProvider logic. Will be called by {@link
     * SplitCompatContentProvider}.
     */
    public abstract static class Impl {
        private SplitCompatContentProvider mContentProvider;

        protected void setContentProvider(SplitCompatContentProvider contentProvider) {
            mContentProvider = contentProvider;
        }

        protected final Context getContext() {
            return mContentProvider.getContext();
        }

        protected final String getCallingPackage() {
            return mContentProvider.getCallingPackage();
        }

        public abstract Cursor query(
                Uri uri,
                String[] projection,
                String selection,
                String[] selectionArgs,
                String sortOrder);

        public abstract Uri insert(Uri uri, ContentValues values);

        public abstract int delete(Uri uri, String selection, String[] selectionArgs);

        public abstract int update(
                Uri uri, ContentValues values, String selection, String[] selectionArgs);

        public abstract String getType(Uri uri);

        public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {}
    }
}
