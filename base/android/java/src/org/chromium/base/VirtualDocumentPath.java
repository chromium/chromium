// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.io.OutputStream;

/**
 * Represents and operates on a virtual path for Android's Storage Access Framework (SAF).
 *
 * <p>For details, see {@code base/android/virtual_document_path.h}.
 */
@NullMarked
class VirtualDocumentPath {
    static class CreateOrOpenResult {
        public final Uri contentUri;
        public final boolean created;

        private CreateOrOpenResult(Uri contentUri, boolean created) {
            this.contentUri = contentUri;
            this.created = created;
        }

        @CalledByNative("CreateOrOpenResult")
        @JniType("std::string")
        String getContentUriString() {
            return this.contentUri.toString();
        }

        @CalledByNative("CreateOrOpenResult")
        boolean getCreated() {
            return this.created;
        }
    }

    private static final String TAG = "VirtualDocumentPath";

    private static final String VIRTUAL_PATH_MARKER = "SAF";
    private static final String TREE_PATH = "tree";

    private final String mAuthority;
    private final String mEncodedTreeId;
    private final String mRelativePath;
    private final ContentResolver mResolver;

    private VirtualDocumentPath(String authority, String encodedTreeId, String relativePath) {
        assert !authority.isEmpty() && !authority.contains("/");
        assert !encodedTreeId.isEmpty() && !encodedTreeId.contains("/");
        assert !relativePath.contains("//")
                && !relativePath.startsWith("/")
                && !relativePath.endsWith("/");

        mAuthority = authority;
        mEncodedTreeId = encodedTreeId;
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
        if (mRelativePath.isEmpty()) {
            return String.format(
                    "/%s/%s/%s/%s", VIRTUAL_PATH_MARKER, mAuthority, TREE_PATH, mEncodedTreeId);
        } else {
            return String.format(
                    "/%s/%s/%s/%s/%s",
                    VIRTUAL_PATH_MARKER, mAuthority, TREE_PATH, mEncodedTreeId, mRelativePath);
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

            // Handle /SAF/<authority>/tree/<documentId>/<relativePath>
            String[] comp = path.split("/", 6);

            if (comp.length < 5) return null;

            if (!comp[0].isEmpty()) return null;
            if (!comp[1].equals(VIRTUAL_PATH_MARKER)) return null;

            String authority = comp[2];
            String pathPart = comp[3];
            String documentId = comp[4];
            // Allow omitting the trailing slash for empty relative path.
            String relativePath = comp.length > 5 ? comp[5] : "";

            if (!pathPart.equals(TREE_PATH)) return null;

            return new VirtualDocumentPath(authority, documentId, relativePath);
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
            String encodedTreeId = Uri.encode(treeId);
            return new VirtualDocumentPath(authority, encodedTreeId, "");
        }
        return null;
    }

    /**
     * Resolves this virtual document path into its corresponding {@code content://} URI string.
     *
     * @return The resolved content URI as a string. Returns an empty string if the file or
     *     directory cannot be found.
     */
    @CalledByNative
    @JniType("std::string")
    String resolveToContentUriString() {
        Uri uri = resolveToContentUri();
        return uri == null ? "" : uri.toString();
    }

    @Nullable
    Uri resolveToContentUri() {
        String[] names = relativePathComponents();

        String treeId = Uri.decode(mEncodedTreeId);
        Uri tree = DocumentsContract.buildTreeDocumentUri(mAuthority, treeId);
        String documentId = DocumentsContract.getTreeDocumentId(tree);
        assert treeId.equals(documentId);

        // TODO(crbug.com/406136787): create a fast path for com.android.externalstorage.documents,
        // which is known to use the relative paths directly as document IDs.

        String[] columns = {
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
        };
        // Use a selection to match displayName, but don't trust that it actually works.
        String selection = String.format("%s = ?", DocumentsContract.Document.COLUMN_DISPLAY_NAME);
        for (String name : names) {
            Uri u = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
            Cursor c;
            try {
                c = mResolver.query(u, columns, selection, new String[] {name}, null);
            } catch (Exception e) {
                return null;
            }
            if (c == null || !c.moveToFirst()) return null;

            int nameIndex = c.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
            int documentIdIndex = c.getColumnIndex(DocumentsContract.Document.COLUMN_DOCUMENT_ID);

            boolean found = false;
            while (true) {
                if (!c.isNull(nameIndex)
                        && !c.isNull(documentIdIndex)
                        && name.equals(c.getString(nameIndex))) {
                    documentId = c.getString(documentIdIndex);
                    found = true;
                    break;
                }
                if (!c.moveToNext()) break;
            }
            c.close();
            if (!found) {
                return null;
            }
        }
        Uri res = DocumentsContract.buildDocumentUriUsingTree(tree, documentId);

        if (names.length == 0) {
            // For an empty relative path, the main traversal loop is skipped, so the URI's
            // current validity isn't verified. Perform the check here to avoid returning
            // a clearly invalid URI. Consider it a performance optimization; the caller is always
            // responsible to handle TOCTOU races.
            if (!contentUriExists(res)) {
                return null;
            }
        }

        return res;
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

        if (resolveToContentUri() != null) return false;

        Uri parentUri = parent.resolveToContentUri();
        if (parentUri == null) return false;
        try {
            Uri dir =
                    DocumentsContract.createDocument(
                            mResolver,
                            parentUri,
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
    @Nullable
    CreateOrOpenResult createOrOpen() {
        Pair<VirtualDocumentPath, String> pair = splitPath();
        if (pair == null) return null;
        VirtualDocumentPath parent = pair.first;
        String basename = pair.second;

        Uri contentUri = resolveToContentUri();
        if (contentUri != null) {
            return new CreateOrOpenResult(contentUri, false);
        }

        Uri parentUri = parent.resolveToContentUri();
        if (parentUri == null) return null;

        try {
            Uri uri = DocumentsContract.createDocument(mResolver, parentUri, "", basename);
            if (uri == null) return null;
            return new CreateOrOpenResult(uri, true);
        } catch (Exception e) {
            Log.w(TAG, "Failed to create file");
            return null;
        }
    }

    private @Nullable Pair<VirtualDocumentPath, String> splitPath() {
        String[] names = relativePathComponents();
        if (names.length == 0) return null;

        String[] parent = new String[names.length - 1];
        System.arraycopy(names, 0, parent, 0, parent.length);
        return new Pair<>(
                new VirtualDocumentPath(mAuthority, mEncodedTreeId, String.join("/", parent)),
                names[names.length - 1]);
    }

    private String[] relativePathComponents() {
        if (mRelativePath.isEmpty()) return new String[0];
        return mRelativePath.split("/");
    }

    private boolean contentUriExists(Uri uri) {
        try {
            Cursor c =
                    mResolver.query(
                            uri,
                            new String[] {
                                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            },
                            null,
                            null,
                            null);
            if (c != null && c.moveToFirst()) {
                return true;
            }
        } catch (Exception e) {
        }
        return false;
    }
}
