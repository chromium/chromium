// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import java.io.IOException;

/** This class provides methods to access content URI schemes. */
@JNINamespace("base")
public abstract class ContentUriUtils {
    private static final String TAG = "ContentUriUtils";

    // Prevent instantiation.
    private ContentUriUtils() {}

    /**
     * Opens the content URI for the specified mode, and returns the file descriptor to the caller.
     * The caller is responsible for closing the file descriptor.
     *
     * @param uriString the content URI to open
     * @param mode the mode to open. Allows all values from ParcelFileDescriptor#parseMode(): ("r",
     *     "w", "wt", "wa", "rw" or "rwt"), but disallows "w" which has been the source of android
     *     security issues.
     * @return file descriptor upon success, or -1 otherwise.
     */
    @CalledByNative
    public static int openContentUri(
            @JniType("std::string") String uriString, @JniType("std::string") String mode) {
        AssetFileDescriptor afd = getAssetFileDescriptor(uriString, mode);
        if (afd != null) {
            return afd.getParcelFileDescriptor().detachFd();
        }
        return -1;
    }

    /**
     * Check whether a content URI exists.
     *
     * @param uriString the content URI to query.
     * @return true if the URI exists, or false otherwise.
     */
    @CalledByNative
    public static boolean contentUriExists(@JniType("std::string") String uriString) {
        AssetFileDescriptor asf = null;
        try {
            asf = getAssetFileDescriptor(uriString, "r");
            if (asf != null) {
                return true;
            }
        } finally {
            // Do not use StreamUtil.closeQuietly here, as AssetFileDescriptor
            // does not implement Closeable until KitKat.
            if (asf != null) {
                try {
                    asf.close();
                } catch (IOException e) {
                    // Closing quietly.
                }
            }
        }

        try {
            DocumentFile file =
                    DocumentFile.fromTreeUri(
                            ContextUtils.getApplicationContext(), Uri.parse(uriString));
            return file != null && file.exists();
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Query with queryUri and populate nativeVector with results.
     *
     * @param uriString the content URI to look up.
     * @param listFiles if true, the children of uri are populated, else uri info is populated.
     * @param nativeVector vector to populate with results via Natives#addFileInfoToVector(). Called
     *     only if file is found.
     */
    private static void populateFileInfo(String uriString, boolean listFiles, long nativeVector) {
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
        };

        Uri queryUri = null;
        try {
            DocumentFile file =
                    DocumentFile.fromTreeUri(
                            ContextUtils.getApplicationContext(), Uri.parse(uriString));
            if (file != null) {
                if (listFiles) {
                    String documentId = DocumentsContract.getDocumentId(file.getUri());
                    queryUri =
                            DocumentsContract.buildChildDocumentsUriUsingTree(
                                    file.getUri(), documentId);
                } else {
                    queryUri = file.getUri();
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to get Documents URI for %s, listFiles=%s", uriString, listFiles);
        }
        if (queryUri == null) {
            // If URI is not a documents URI, then try to get size from AFD.
            if (!listFiles) {
                AssetFileDescriptor afd = getAssetFileDescriptor(uriString, "r");
                if (afd != null) {
                    ContentUriUtilsJni.get()
                            .addFileInfoToVector(
                                    nativeVector, uriString, null, false, afd.getLength(), 0);
                    StreamUtil.closeQuietly(afd);
                }
            }
            return;
        }

        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        try (Cursor c = resolver.query(queryUri, columns, null, null, null)) {
            while (c.moveToNext()) {
                String uri =
                        c.isNull(0)
                                ? null
                                : DocumentsContract.buildDocumentUriUsingTree(
                                                queryUri, c.getString(0))
                                        .toString();
                String displayName = c.isNull(1) ? null : c.getString(1);
                boolean isDirectory =
                        !c.isNull(2)
                                && DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2));
                long size = c.isNull(3) ? 0 : c.getLong(3);
                long lastModified = c.isNull(4) ? 0 : c.getLong(4);
                ContentUriUtilsJni.get()
                        .addFileInfoToVector(
                                nativeVector, uri, displayName, isDirectory, size, lastModified);
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed query for uri=" + uriString + ", listFiles=" + listFiles, e);
        }
    }

    /**
     * Provides file info of the given content URI if file is found. Files can report size of -1
     * when length is unknown.
     *
     * @param uriString the content URI to look up.
     * @param nativeVector vector to populate with results via Natives#addFileInfoToVector(). Called
     *     once if file is found, else not called.
     */
    @CalledByNative
    private static void getFileInfo(@JniType("std::string") String uriString, long nativeVector) {
        populateFileInfo(uriString, false, nativeVector);
    }

    /**
     * Provices an array of files and directories contained in the given directory.
     *
     * @param uriString the content URI to look up.
     * @param nativeVector vector to populate with results via Natives#addFileInfoToVector(). Called
     *     for each file in this directory.
     */
    @CalledByNative
    private static void listDirectory(@JniType("std::string") String uriString, long nativeVector) {
        populateFileInfo(uriString, true, nativeVector);
    }

    /**
     * Retrieve the MIME type for the content URI.
     *
     * @param uriString the content URI to look up.
     * @return MIME type or null if the input params are empty or invalid.
     */
    @Nullable
    @CalledByNative
    public static String getMimeType(@JniType("std::string") String uriString) {
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        Uri uri = Uri.parse(uriString);
        if (isVirtualDocument(uri)) {
            String[] streamTypes = resolver.getStreamTypes(uri, "*/*");
            return (streamTypes != null && streamTypes.length > 0) ? streamTypes[0] : null;
        }
        return resolver.getType(uri);
    }

    /**
     * Helper method to open a content URI and returns the ParcelFileDescriptor.
     *
     * @param uriString the content URI to open.
     * @param mode the mode to open. Allows all values from ParcelFileDescriptor#parseMode(): ("r",
     *     "w", "wt", "wa", "rw" or "rwt"), but disallows "w" which has been the source of android
     *     security issues.
     * @return AssetFileDescriptor of the content URI, or NULL if the file does not exist.
     */
    @Nullable
    private static AssetFileDescriptor getAssetFileDescriptor(String uriString, String mode) {
        if ("w".equals(mode)) {
            Log.e(TAG, "Cannot open files with mode 'w'");
            return null;
        }

        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        Uri uri = Uri.parse(uriString);

        try {
            AssetFileDescriptor afd = null;
            if (isVirtualDocument(uri)) {
                String[] streamTypes = resolver.getStreamTypes(uri, "*/*");
                if (streamTypes != null && streamTypes.length > 0) {
                    afd = resolver.openTypedAssetFileDescriptor(uri, streamTypes[0], null);
                }
            } else {
                afd = resolver.openAssetFileDescriptor(uri, mode);
            }
            if (afd != null && afd.getStartOffset() != 0) {
                StreamUtil.closeQuietly(afd);
                throw new SecurityException("Cannot open files with non-zero offset type.");
            }
            return afd;
        } catch (Exception e) {
            Log.w(TAG, "Cannot open content uri: %s", uriString, e);
        }
        return null;
    }

    /**
     * Method to resolve the display name of a content URI.
     *
     * @param uri the content URI to be resolved.
     * @param context {@link Context} in interest.
     * @param columnField the column field to query.
     * @return the display name of the @code uri if present in the database or an empty string
     *     otherwise.
     */
    public static String getDisplayName(Uri uri, Context context, String columnField) {
        if (uri == null) return "";

        // For a URI without a document ID such as a directory which is a tree URI /tree/<treeId>,
        // we need to get a URI which includes /tree/<treeId>/document/<docId>.
        if (DocumentsContract.isTreeUri(uri) && !DocumentsContract.isDocumentUri(context, uri)) {
            try {
                uri = DocumentFile.fromTreeUri(context, uri).getUri();
            } catch (Exception e) {
                // Ignore if URI fails to build, and attempt with the existing uri.
            }
        }

        ContentResolver contentResolver = context.getContentResolver();
        try (Cursor cursor = contentResolver.query(uri, null, null, null, null)) {
            if (cursor != null && cursor.getCount() >= 1) {
                cursor.moveToFirst();
                int displayNameIndex = cursor.getColumnIndex(columnField);
                if (displayNameIndex == -1) {
                    return "";
                }
                String displayName = cursor.getString(displayNameIndex);
                // For Virtual documents, try to modify the file extension so it's compatible
                // with the alternative MIME type.
                if (hasVirtualFlag(cursor)) {
                    String[] mimeTypes = contentResolver.getStreamTypes(uri, "*/*");
                    if (mimeTypes != null && mimeTypes.length > 0) {
                        String ext =
                                MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeTypes[0]);
                        if (ext != null) {
                            // Just append, it's simpler and more secure than altering an
                            // existing extension.
                            displayName += "." + ext;
                        }
                    }
                }
                return displayName;
            }
        } catch (NullPointerException e) {
            // Some android models don't handle the provider call correctly.
            // see crbug.com/345393
            return "";
        } catch (UnsupportedOperationException e) {
            // Fails for URIs such as a directory tree URI without a document ID.
            Log.w(TAG, "Cannot get display name for %s", uri, e);
            return "";
        }
        return "";
    }

    /**
     * Method to resolve the display name of a content URI if possible.
     *
     * @param uriString the content URI to look up.
     * @return the display name of the uri if present in the database or null otherwise.
     */
    @Nullable
    @CalledByNative
    public static String maybeGetDisplayName(@JniType("std::string") String uriString) {
        Uri uri = Uri.parse(uriString);

        try {
            String displayName =
                    getDisplayName(
                            uri,
                            ContextUtils.getApplicationContext(),
                            MediaStore.MediaColumns.DISPLAY_NAME);
            return TextUtils.isEmpty(displayName) ? null : displayName;
        } catch (Exception e) {
            // There are a few Exceptions we can hit here (e.g. SecurityException), but we don't
            // particularly care what kind of Exception we hit. If we hit one, just don't return a
            // display name.
            Log.w(TAG, "Cannot open content uri: %s", uriString, e);
        }

        // If we are unable to query the content URI, just return null.
        return null;
    }

    /**
     * Checks whether the passed Uri represents a virtual document.
     *
     * @param uri the content URI to be resolved.
     * @return True for virtual file, false for any other file.
     */
    private static boolean isVirtualDocument(Uri uri) {
        if (uri == null) return false;
        if (!DocumentsContract.isDocumentUri(ContextUtils.getApplicationContext(), uri)) {
            return false;
        }
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        try (Cursor cursor = contentResolver.query(uri, null, null, null, null)) {
            if (cursor != null && cursor.getCount() >= 1) {
                cursor.moveToFirst();
                return hasVirtualFlag(cursor);
            }
        } catch (NullPointerException e) {
            // Some android models don't handle the provider call correctly.
            // see crbug.com/345393
            return false;
        }
        return false;
    }

    /**
     * Checks whether the passed cursor for a document has a virtual document flag.
     *
     * The called must close the passed cursor.
     *
     * @param cursor Cursor with COLUMN_FLAGS.
     * @return True for virtual file, false for any other file.
     */
    private static boolean hasVirtualFlag(Cursor cursor) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return false;
        int index = cursor.getColumnIndex(DocumentsContract.Document.COLUMN_FLAGS);
        return index > -1
                && (cursor.getLong(index) & DocumentsContract.Document.FLAG_VIRTUAL_DOCUMENT) != 0;
    }

    /**
     * @return whether a Uri has content scheme.
     */
    public static boolean isContentUri(String uri) {
        if (uri == null) return false;
        Uri parsedUri = Uri.parse(uri);
        return parsedUri != null && ContentResolver.SCHEME_CONTENT.equals(parsedUri.getScheme());
    }

    /**
     * Deletes a content uri from the system.
     *
     * @return True if the uri was deleted.
     */
    @CalledByNative
    public static boolean delete(@JniType("std::string") String uriString) {
        assert isContentUri(uriString);
        Uri parsedUri = Uri.parse(uriString);
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        try {
            if (DocumentsContract.deleteDocument(resolver, parsedUri)) {
                return true;
            }
        } catch (Exception e) {
            Log.w(TAG, "DocumentsContract could not delete %s: %s", uriString, e.getMessage());
        }
        try {
            if (resolver.delete(parsedUri, null, null) > 0) {
                return true;
            }
        } catch (Exception e) {
            Log.w(TAG, "ContentResolver could not delete %s: %s", uriString, e.getMessage());
        }
        return false;
    }

    /**
     * Build URI using treeUri and encodedDocumentId.
     *
     * @param treeUri URI of directory.
     * @param encodedDocumentId URL-encoded documentID of file.
     * @return uri
     * @see DocumentsContract#buildDocumentUriUsingTree(Uri, String)
     */
    @Nullable
    @CalledByNative
    public static String buildDocumentUriUsingTree(
            @JniType("std::string") String treeUri,
            @JniType("std::string") String encodedDocumentId) {
        try {
            return DocumentsContract.buildDocumentUriUsingTree(
                            Uri.parse(treeUri), Uri.decode(encodedDocumentId))
                    .toString();
        } catch (Exception e) {
            return null;
        }
    }

    @NativeMethods
    interface Natives {
        void addFileInfoToVector(
                long vectorPointer,
                String uri,
                String displayName,
                boolean isDirectory,
                long size,
                long lastModified);
    }
}
