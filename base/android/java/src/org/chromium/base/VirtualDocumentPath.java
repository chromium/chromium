// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.OutputStream;
import java.util.Arrays;

/**
 * Represents and operates on a virtual path for Android's Storage Access Framework (SAF).
 *
 * <p>For details, see {@code base/android/virtual_document_path.h}.
 */
@NullMarked
class VirtualDocumentPath {
    static class ResolveResult {
        public final @FileError int status;
        public final @Nullable Uri contentUri;
        public final boolean isDirectory;

        ResolveResult(@FileError int status, @Nullable Uri contentUri, boolean isDirectory) {
            this.status = status;
            this.contentUri = contentUri;
            this.isDirectory = isDirectory;
        }

        @CalledByNative
        @JniType("base::File::Error")
        @FileError
        int getStatus() {
            return this.status;
        }

        @CalledByNative
        @JniType("std::string")
        String getContentUriString() {
            return this.contentUri != null ? this.contentUri.toString() : "";
        }

        @CalledByNative
        boolean isDirectory() {
            return this.isDirectory;
        }
    }

    static class CreateOrOpenResult {
        public final Uri contentUri;
        public final boolean created;

        private CreateOrOpenResult(Uri contentUri, boolean created) {
            this.contentUri = contentUri;
            this.created = created;
        }

        @CalledByNative
        @JniType("std::string")
        String getContentUriString() {
            return this.contentUri.toString();
        }

        @CalledByNative
        boolean getCreated() {
            return this.created;
        }
    }

    private static final String TAG = "VirtualDocumentPath";

    private static final String VIRTUAL_PATH_MARKER = "SAF";
    private static final String TREE_PATH = "tree";

    private final String mAuthority;
    private final String mTreeId;
    private final String[] mRelativePath;
    private final ContentResolver mResolver;

    private VirtualDocumentPath(String authority, String treeId, String[] relativePath) {
        assert !authority.isEmpty() && !authority.contains("/");
        assert !treeId.isEmpty();

        mAuthority = authority;
        mTreeId = treeId;
        mRelativePath = relativePath;

        mResolver = ContextUtils.getApplicationContext().getContentResolver();
    }

    /**
     * Returns string representation of the instance. See the class level comment for details.
     *
     * @return The string representation of the {@link VirtualDocumentPath}.
     */
    @Override
    @CalledByNative
    @JniType("std::string")
    public String toString() {
        String encodedTreeId = Uri.encode(mTreeId);
        if (mRelativePath.length == 0) {
            return String.format(
                    "/%s/%s/%s/%s", VIRTUAL_PATH_MARKER, mAuthority, TREE_PATH, encodedTreeId);
        } else {
            return String.format(
                    "/%s/%s/%s/%s/%s",
                    VIRTUAL_PATH_MARKER,
                    mAuthority,
                    TREE_PATH,
                    encodedTreeId,
                    String.join("/", mRelativePath));
        }
    }

    /**
     * Parses virtual path "/SAF/..." to {@link VirtualDocumentPath} or resolves a tree URI (a
     * content URI that represents a document tree) into {@link VirtualDocumentPath}. See {@link
     * https://developer.android.com/reference/android/provider/DocumentsContract} for more about
     * document tree URIs.
     *
     * @param path The virtual path string to parse.
     * @return A {@link VirtualDocumentPath} object, or null if parsing fails.
     */
    @CalledByNative
    static @Nullable VirtualDocumentPath parse(@JniType("std::string") String path) {
        if (path.startsWith("/")) {
            if (path.contains("//")) return null;
            if (path.endsWith("/")) {
                path = path.substring(0, path.length() - 1);
            }

            // Handle /SAF/<authority>/tree/<encodedTreeId>/<relativePath>
            String[] comp = path.split("/", 6);

            if (comp.length < 5) return null;

            if (!comp[0].isEmpty()) return null;
            if (!comp[1].equals(VIRTUAL_PATH_MARKER)) return null;

            String authority = comp[2];
            String pathPart = comp[3];
            String encodedTreeId = comp[4];
            String[] relativePath = comp.length > 5 ? comp[5].split("/") : new String[0];

            if (!pathPart.equals(TREE_PATH)) return null;
            if (authority.isEmpty() || encodedTreeId.isEmpty()) return null;

            for (String s : relativePath) {
                if (s.isEmpty() || s.equals(".") || s.equals("..") || s.indexOf('\0') != -1) {
                    return null;
                }
            }

            return new VirtualDocumentPath(authority, Uri.decode(encodedTreeId), relativePath);
        }
        if (path.startsWith("content://")) {
            // Handle document tree.
            Uri uri = Uri.parse(path);
            if (!DocumentsContract.isTreeUri(uri)) return null;
            try {
                // Return null if the URI refers to a document, not just a tree.
                DocumentsContract.getDocumentId(uri);
                return null;
            } catch (IllegalArgumentException e) {
            }
            String authority = uri.getAuthority();
            if (authority == null || authority.isEmpty()) return null;
            String treeId = DocumentsContract.getTreeDocumentId(uri);
            return new VirtualDocumentPath(authority, treeId, new String[0]);
        }
        return null;
    }

    /**
     * Resolves this virtual document path into its corresponding {@code content://} URI string.
     *
     * @return The resolved content URI as a string. Returns an empty string if the file or
     *     directory cannot be found.
     */
    String resolveToContentUriString() {
        Uri uri = resolveToContentUri();
        return uri == null ? "" : uri.toString();
    }

    @Nullable Uri resolveToContentUri() {
        ResolveResult res = resolveDocument();
        return res.status == FileError.FILE_OK ? res.contentUri : null;
    }

    /**
     * Resolves this virtual document path. A null cursor, or a cursor flagged {@link
     * DocumentsContract#EXTRA_LOADING}, yields {@link FileError#FILE_ERROR_FAILED} rather than
     * {@link FileError#FILE_ERROR_NOT_FOUND} or {@link FileError#FILE_OK}, even when a matching row
     * was found: an incomplete listing can neither prove absence nor rule out a duplicate, so
     * callers never create a document based on it. This applies to the tree root query as well as
     * to each child listing. Duplicate matching rows or unsupported provider queries yield {@link
     * FileError#FILE_ERROR_INVALID_OPERATION}.
     */
    @CalledByNative
    ResolveResult resolveDocument() {
        Uri tree = DocumentsContract.buildTreeDocumentUri(mAuthority, mTreeId);
        Uri rootUri = DocumentsContract.buildDocumentUriUsingTree(tree, mTreeId);
        ResolveResult current = queryDocument(tree, rootUri, null);
        for (String name : mRelativePath) {
            if (current.status != FileError.FILE_OK || current.contentUri == null) {
                return current;
            }
            if (!current.isDirectory) {
                return new ResolveResult(FileError.FILE_ERROR_NOT_A_DIRECTORY, null, false);
            }
            current = resolveChild(current.contentUri, name);
        }
        return current;
    }

    private ResolveResult resolveChild(Uri parentDocumentUri, String name) {
        Uri tree = DocumentsContract.buildTreeDocumentUri(mAuthority, mTreeId);
        String parentDocId = DocumentsContract.getDocumentId(parentDocumentUri);
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentDocId);
        return queryDocument(tree, childrenUri, name);
    }

    private ResolveResult queryDocument(Uri tree, Uri queryUri, @Nullable String displayName) {
        String[] columns = {
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
        };
        // Use a selection to match displayName, but don't trust that it actually works.
        final String selection =
                displayName != null
                        ? DocumentsContract.Document.COLUMN_DISPLAY_NAME + " = ?"
                        : null;
        String[] selectionArgs = displayName != null ? new String[] {displayName} : null;
        try (Cursor c = mResolver.query(queryUri, columns, selection, selectionArgs, null)) {
            if (c == null) {
                return new ResolveResult(FileError.FILE_ERROR_FAILED, null, false);
            }

            int nameIndex = c.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
            int documentIdIndex = c.getColumnIndex(DocumentsContract.Document.COLUMN_DOCUMENT_ID);
            int mimeTypeIndex = c.getColumnIndex(DocumentsContract.Document.COLUMN_MIME_TYPE);
            if ((displayName != null && nameIndex == -1) || documentIdIndex == -1) {
                return new ResolveResult(FileError.FILE_ERROR_FAILED, null, false);
            }

            String matchedDocId = null;
            boolean isDir = false;
            while (c.moveToNext()) {
                if (c.isNull(documentIdIndex)) {
                    continue;
                }
                if (displayName != null
                        && (c.isNull(nameIndex) || !displayName.equals(c.getString(nameIndex)))) {
                    continue;
                }
                if (matchedDocId != null) {
                    return new ResolveResult(FileError.FILE_ERROR_INVALID_OPERATION, null, false);
                }
                matchedDocId = c.getString(documentIdIndex);
                isDir =
                        mimeTypeIndex != -1
                                && !c.isNull(mimeTypeIndex)
                                && DocumentsContract.Document.MIME_TYPE_DIR.equals(
                                        c.getString(mimeTypeIndex));
            }

            if (isCursorLoading(c)) {
                return new ResolveResult(FileError.FILE_ERROR_FAILED, null, false);
            }
            if (matchedDocId == null) {
                return new ResolveResult(FileError.FILE_ERROR_NOT_FOUND, null, false);
            }

            Uri resolvedUri = DocumentsContract.buildDocumentUriUsingTree(tree, matchedDocId);
            return new ResolveResult(FileError.FILE_OK, resolvedUri, isDir);
        } catch (SecurityException e) {
            return new ResolveResult(FileError.FILE_ERROR_ACCESS_DENIED, null, false);
        } catch (UnsupportedOperationException | IllegalArgumentException e) {
            return new ResolveResult(FileError.FILE_ERROR_INVALID_OPERATION, null, false);
        } catch (Exception e) {
            return new ResolveResult(FileError.FILE_ERROR_FAILED, null, false);
        }
    }

    /**
     * Makes directory represented by the virtual path. If the file already exists, it does nothing
     * and returns false.
     *
     * @return Whether the directory has been successfully created.
     */
    @CalledByNative
    boolean mkdir() {
        Pair<VirtualDocumentPath, String> pair = splitPath();
        if (pair == null) return false;
        VirtualDocumentPath parent = pair.first;
        String basename = pair.second;

        ResolveResult parentRes = parent.resolveDocument();
        if (parentRes.status != FileError.FILE_OK
                || parentRes.contentUri == null
                || !parentRes.isDirectory) {
            return false;
        }

        ResolveResult existing = resolveChild(parentRes.contentUri, basename);
        if (existing.status != FileError.FILE_ERROR_NOT_FOUND) return false;

        try {
            Uri dir =
                    DocumentsContract.createDocument(
                            mResolver,
                            parentRes.contentUri,
                            DocumentsContract.Document.MIME_TYPE_DIR,
                            basename);
            return dir != null;
        } catch (Exception e) {
            Log.w(TAG, "Failed to create directory");
            return false;
        }
    }

    /**
     * Writes data to the file represented by the virtual path. If the file already exists its
     * content is truncated first.
     *
     * @param data The data to write.
     * @return Whether the data has been successfully written.
     */
    @CalledByNative
    boolean writeFile(byte[] data) {
        CreateOrOpenResult result = createOrOpen();
        if (result == null) return false;

        try (OutputStream out = mResolver.openOutputStream(result.contentUri)) {
            if (out == null) return false;
            out.write(data);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Failed to write to " + result.contentUri);
            return false;
        }
    }

    /**
     * Creates an empty file if it does not exist and its parent directory exists. It returns the
     * content uri if the file exists or created.
     */
    @CalledByNative
    @Nullable CreateOrOpenResult createOrOpen() {
        Pair<VirtualDocumentPath, String> pair = splitPath();
        if (pair == null) {
            ResolveResult resolved = resolveDocument();
            if (resolved.status != FileError.FILE_OK
                    || resolved.isDirectory
                    || resolved.contentUri == null) {
                return null;
            }
            return new CreateOrOpenResult(resolved.contentUri, false);
        }
        ResolveResult parentRes = pair.first.resolveDocument();
        if (parentRes.status != FileError.FILE_OK
                || parentRes.contentUri == null
                || !parentRes.isDirectory) {
            return null;
        }

        ResolveResult existing = resolveChild(parentRes.contentUri, pair.second);
        if (existing.status == FileError.FILE_OK) {
            if (existing.isDirectory || existing.contentUri == null) {
                return null;
            }
            return new CreateOrOpenResult(existing.contentUri, false);
        }
        if (existing.status != FileError.FILE_ERROR_NOT_FOUND) {
            return null;
        }

        try {
            Uri uri =
                    DocumentsContract.createDocument(
                            mResolver, parentRes.contentUri, "", pair.second);
            if (uri == null) return null;
            return new CreateOrOpenResult(uri, true);
        } catch (Exception e) {
            Log.w(TAG, "Failed to create file");
            return null;
        }
    }

    private static boolean isCursorLoading(Cursor cursor) {
        Bundle extras = cursor.getExtras();
        return extras != null && extras.getBoolean(DocumentsContract.EXTRA_LOADING, false);
    }

    private @Nullable Pair<VirtualDocumentPath, String> splitPath() {
        if (mRelativePath.length == 0) return null;

        String[] parent = Arrays.copyOf(mRelativePath, mRelativePath.length - 1);
        return new Pair<>(
                new VirtualDocumentPath(mAuthority, mTreeId, parent),
                mRelativePath[mRelativePath.length - 1]);
    }
}
