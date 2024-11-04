// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentProvider;
import android.content.Context;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsProvider;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

/**
 * DocumentsProvider that reads files into memory when requested and returns an AssetFileDescriptor
 * to the memory-backed contents rather than to the local file. This is used for testing how
 * DocumentProviders which do not use local files will behave.
 */
public class TestDocumentsProvider extends DocumentsProvider {
    private static final String AUTHORITY = "org.chromium.native_test.docprov";

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
                        // Ignore.
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

    public File getFile(String documentId) throws FileNotFoundException {
        try {
            String pathUnderCacheDir = URLDecoder.decode(documentId, "UTF-8");
            return new File(mCacheDir, pathUnderCacheDir);
        } catch (UnsupportedEncodingException e) {
            throw new FileNotFoundException(documentId);
        }
    }

    public String getDocumentId(File file) {
        return file.getAbsolutePath().substring(mCacheDir.getAbsolutePath().length() + 1);
    }

    public Cursor query(String[] projection, File[] files) {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : new String[0]);
        for (File file : files) {
            if (file.exists()) {
                Object[] row = new Object[projection != null ? projection.length : 0];
                for (int i = 0; i < projection.length; i++) {
                    String colName = projection[i];
                    if (DocumentsContract.Document.COLUMN_DOCUMENT_ID.equals(colName)) {
                        row[i] = getDocumentId(file);
                    } else if (DocumentsContract.Document.COLUMN_DISPLAY_NAME.equals(colName)) {
                        row[i] = file.toPath().getFileName();
                    } else if (DocumentsContract.Document.COLUMN_MIME_TYPE.equals(colName)) {
                        row[i] = file.isDirectory() ? DocumentsContract.Document.MIME_TYPE_DIR : "";
                    } else if (DocumentsContract.Document.COLUMN_SIZE.equals(colName)) {
                        row[i] = file.length();
                    } else if (DocumentsContract.Document.COLUMN_LAST_MODIFIED.equals(colName)) {
                        row[i] = file.lastModified();
                    }
                }
                cursor.addRow(row);
            }
        }
        return cursor;
    }

    @Override
    public String createDocument(String parentDocumentId, String mimeType, String displayName)
            throws FileNotFoundException {
        boolean isDirectory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);
        File parent = getFile(parentDocumentId);
        File child = new File(parent, displayName);
        try {
            if (isDirectory) {
                child.mkdirs();
            } else {
                parent.mkdirs();
                child.createNewFile();
            }
        } catch (Exception e) {
            throw new FileNotFoundException(e.getMessage());
        }
        return getDocumentId(child);
    }

    @Override
    public ParcelFileDescriptor openDocument(
            String documentId, String mode, CancellationSignal signal)
            throws FileNotFoundException {
        try (FileInputStream fis = new FileInputStream(getFile(documentId))) {
            byte[] buf = FileUtils.readStream(fis);
            return openPipeHelper(
                    DocumentsContract.buildDocumentUri(AUTHORITY, documentId),
                    null,
                    null,
                    buf,
                    mPipeDataWriter);
        } catch (IOException ioe) {
            throw new FileNotFoundException("Error opening " + documentId);
        }
    }

    @Override
    public Cursor queryChildDocuments(
            String parentDocumentId, String[] projection, String sortOrder)
            throws FileNotFoundException {
        return query(projection, getFile(parentDocumentId).listFiles());
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection)
            throws FileNotFoundException {
        return query(projection, new File[] {getFile(documentId)});
    }

    @Override
    public Cursor queryRoots(String[] projection) {
        return null;
    }

    @Override
    public boolean isChildDocument(String parentDocumentId, String documentId) {
        return documentId.startsWith(parentDocumentId + "/");
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        getFile(documentId).delete();
    }
}
