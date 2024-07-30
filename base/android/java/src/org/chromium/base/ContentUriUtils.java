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

import org.jni_zero.CalledByNative;

import java.io.File;
import java.io.IOException;

/** This class provides methods to access content URI schemes. */
public abstract class ContentUriUtils {
    private static final String TAG = "ContentUriUtils";
    private static FileProviderUtil sFileProviderUtil;

    // Guards access to sFileProviderUtil.
    private static final Object sLock = new Object();

    /**
     * Provides functionality to translate a file into a content URI for use
     * with a content provider.
     */
    public interface FileProviderUtil {
        /**
         * Generate a content URI from the given file.
         *
         * @param file The file to be translated.
         */
        Uri getContentUriFromFile(File file);
    }

    // Prevent instantiation.
    private ContentUriUtils() {}

    public static void setFileProviderUtil(FileProviderUtil util) {
        synchronized (sLock) {
            sFileProviderUtil = util;
        }
    }

    /**
     * Get a URI for |file| which has the image capture. This function assumes that path of |file|
     * is based on the result of UiUtils.getDirectoryForImageCapture().
     *
     * @param file image capture file.
     * @return URI for |file|.
     * @throws IllegalArgumentException when the given File is outside the paths supported by the
     *         provider.
     */
    public static Uri getContentUriFromFile(File file) {
        synchronized (sLock) {
            if (sFileProviderUtil != null) {
                return sFileProviderUtil.getContentUriFromFile(file);
            }
        }
        return null;
    }

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
    public static int openContentUri(String uriString, String mode) {
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
    public static boolean contentUriExists(String uriString) {
        AssetFileDescriptor asf = null;
        try {
            asf = getAssetFileDescriptor(uriString, "r");
            return asf != null;
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
    }

    /**
     * Get the file size of a content URI.
     *
     * @param uriString the content URI to look up.
     * @return File size or -1 if the file does not exist.
     */
    @CalledByNative
    public static long getContentUriFileSize(String uriString) {
        long size = -1;
        AssetFileDescriptor afd = getAssetFileDescriptor(uriString, "r");
        if (afd != null) {
            size = afd.getLength();
        }
        StreamUtil.closeQuietly(afd);
        return size;
    }

    /**
     * Retrieve the MIME type for the content URI.
     *
     * @param uriString the content URI to look up.
     * @return MIME type or null if the input params are empty or invalid.
     */
    @CalledByNative
    public static String getMimeType(String uriString) {
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
     * @param uri         the content URI to be resolved.
     * @param context     {@link Context} in interest.
     * @param columnField the column field to query.
     * @return the display name of the @code uri if present in the database
     * or an empty string otherwise.
     */
    public static String getDisplayName(Uri uri, Context context, String columnField) {
        if (uri == null) return "";
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
    public static String maybeGetDisplayName(String uriString) {
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
    public static boolean delete(String uriString) {
        assert isContentUri(uriString);
        Uri parsedUri = Uri.parse(uriString);
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        return resolver.delete(parsedUri, null, null) > 0;
    }

    /**
     * Retrieve the content URI from the file path.
     *
     * @param filePathString the file path.
     * @return content URI or null if the input params are invalid.
     */
    @CalledByNative
    public static String getContentUriFromFilePath(String filePathString) {
        try {
            Uri contentUri = getContentUriFromFile(new File(filePathString));
            if (contentUri != null) {
                return contentUri.toString();
            }
        } catch (IllegalArgumentException e) {
            // This happens when the given File is outside the paths supported by the provider.
            Log.e(TAG, "Cannot retrieve content uri from file: %s", filePathString, e);
        }
        return null;
    }
}
