// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.ProviderInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

/**
 * ContentProvider that reads files into memory when requested and returns an AssetFileDescriptor to
 * the memory-backed contents rather than to the local file. This is used for testing how
 * ContentProviders which do not use local files will behave.
 */
public class InMemoryContentProvider extends ContentProvider {
    private static final String PREFIX = "content://org.chromium.native_test.inmemory/cache/";

    private PipeDataWriter mPipeDataWriter =
            new ContentProvider.PipeDataWriter<byte[]>() {
                @Override
                public void writeDataToPipe(
                        ParcelFileDescriptor output,
                        Uri uri,
                        String mimeType,
                        Bundle opts,
                        byte[] imageBytes) {
                    try (OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
                        out.write(imageBytes);
                        out.flush();
                    } catch (Exception e) {
                    }
                }
            };

    private File mCacheDir;

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public void attachInfo(Context context, ProviderInfo info) {
        super.attachInfo(context, info);
        mCacheDir = context.getCacheDir();
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        MatrixCursor cursor =
                new MatrixCursor(
                        new String[] {
                            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                            DocumentsContract.Document.COLUMN_MIME_TYPE,
                            DocumentsContract.Document.COLUMN_SIZE,
                            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
                        },
                        1);

        String documentId;
        String pathUnderCacheDir;
        try {
            documentId = DocumentsContract.getDocumentId(uri);
            pathUnderCacheDir = URLDecoder.decode(documentId, "UTF-8");
        } catch (UnsupportedEncodingException uee) {
            return cursor;
        }
        File file = new File(mCacheDir, pathUnderCacheDir);
        if (file.exists()) {
            cursor.addRow(
                    new Object[] {
                        documentId,
                        file.toPath().getFileName(),
                        file.isDirectory() ? DocumentsContract.Document.MIME_TYPE_DIR : "",
                        file.length(),
                        file.lastModified()
                    });
        }
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
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
    public String getType(Uri uri) {
        throw new UnsupportedOperationException();
    }

    @Override
    public AssetFileDescriptor openAssetFile(Uri uri, String mode)
            throws FileNotFoundException, SecurityException {
        String pathUnderCacheDir;
        if (uri.toString().startsWith(PREFIX)) {
            pathUnderCacheDir = uri.toString().substring(PREFIX.length());
        } else {
            try {
                String documentId = DocumentsContract.getDocumentId(uri);
                pathUnderCacheDir = URLDecoder.decode(documentId, "UTF-8");
            } catch (UnsupportedEncodingException uee) {
                throw new SecurityException("Invalid uri " + uri);
            }
        }
        File file = new File(mCacheDir, pathUnderCacheDir);
        try (FileInputStream fis = new FileInputStream(file)) {
            byte[] buf = FileUtils.readStream(fis);
            ParcelFileDescriptor fd = openPipeHelper(uri, null, null, buf, mPipeDataWriter);
            return new AssetFileDescriptor(fd, 0, file.length());
        } catch (IOException ioe) {
            throw new SecurityException("Error reading " + uri, ioe);
        }
    }
}
