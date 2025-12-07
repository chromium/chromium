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
 * A {@link DocumentsProvider} implementation for testing purposes that simulates a provider that
 * does not grant direct file access for read operations.
 *
 * <p><b>Behavior:</b>
 *
 * <ul>
 *   <li><b>File Storage:</b> All files and directories are physically stored in the application's
 *       cache directory (see {@link Context#getCacheDir()}).
 *   <li><b>Document ID:</b> The {@code documentId} for any file or directory is its relative path
 *       from the cache directory.
 *   <li><b>Read Operations:</b> When a document is opened for reading, this provider reads the
 *       entire file content into a memory buffer and streams it back to the client through a pipe
 *       ({@link ContentProvider#openPipeHelper}). This is useful for testing client code against
 *       providers (e.g., cloud storage providers) that do not return a direct file descriptor to a
 *       local file.
 *   <li><b>Write Operations:</b> When a document is opened for writing, a direct file descriptor to
 *       the underlying file in the cache directory is returned.
 *   <li><b>Authority:</b> The provider's authority is dynamically constructed as {@code
 *       <app_package_name>.docprov}.
 * </ul>
 */
public class TestDocumentsProvider extends DocumentsProvider {
    // TODO(crbug.com/443222522): Implement COLUMN_FLAGS projection
    private static final String[] DEFAULT_PROJECTION_FOR_TEST =
            new String[] {
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_LAST_MODIFIED,
            };

    // The authority cannot be static as its initialization depends on a Context. A ContentProvider
    // can be created before Application.onCreate() is called, which is where the static
    // application context in ContextUtils is set. Therefore, we must wait until the provider's
    // own onCreate() is called to safely get a context.
    private String mAuthority;

    private final PipeDataWriter mPipeDataWriter =
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
        mAuthority = getContext().getPackageName() + ".docprov";
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

    /**
     * Return all available columns when the projection is null to follow the contract of
     * DocumentsProvider#queryDocument and so on
     * https://developer.android.com/reference/android/provider/DocumentsProvider.
     */
    public Cursor query(String[] projection, File[] files) {
        String[] resolvedProjection = projection == null ? DEFAULT_PROJECTION_FOR_TEST : projection;
        MatrixCursor cursor = new MatrixCursor(resolvedProjection);
        for (File file : files) {
            if (file.exists()) {
                Object[] row = new Object[resolvedProjection.length];
                for (int i = 0; i < resolvedProjection.length; i++) {
                    String colName = resolvedProjection[i];
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
        if (mode.contains("w")) {
            File file = getFile(documentId);
            if (file.isDirectory()) {
                throw new FileNotFoundException("Cannot open a directory with mode: " + mode);
            }
            int accessMode = ParcelFileDescriptor.parseMode(mode);
            return ParcelFileDescriptor.open(file, accessMode);
        }

        try (FileInputStream fis = new FileInputStream(getFile(documentId))) {
            byte[] buf = FileUtils.readStream(fis);
            return openPipeHelper(
                    DocumentsContract.buildDocumentUri(mAuthority, documentId),
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
