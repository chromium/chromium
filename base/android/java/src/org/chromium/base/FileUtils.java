// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore.Downloads;
import android.provider.MediaStore.MediaColumns;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.function.Function;

/** Helper methods for dealing with Files. */
@NullMarked
@JNINamespace("base::android")
public class FileUtils {
    private static final String TAG = "FileUtils";

    public static Function<String, Boolean> DELETE_ALL = filepath -> true;

    /**
     * Delete the given File and (if it's a directory) everything within it. Caveat: Return values
     * from recursive deletes are ignored.
     *
     * @param currentFile The file or directory to delete. Does not need to exist.
     * @param canDelete The {@link Function} function used to check if the file can be deleted.
     *     Caveat: {@code canDelete} is not robust, see https://crbug.com/1066733.
     * @return True if the files are deleted, or files reserved by |canDelete|, false if failed to
     *     delete files.
     */
    public static boolean recursivelyDeleteFile(
            File currentFile, Function<String, Boolean> canDelete) {
        if (!currentFile.exists()) {
            // This file could be a broken symlink, so try to delete. If we don't delete a broken
            // symlink, the directory containing it cannot be deleted.
            currentFile.delete();
            return true;
        }
        if (canDelete != null && !canDelete.apply(currentFile.getPath())) {
            return true;
        }

        if (currentFile.isDirectory()) {
            File[] files = currentFile.listFiles();
            if (files != null) {
                for (var file : files) {
                    recursivelyDeleteFile(file, canDelete);
                }
            }
        }

        boolean ret = currentFile.delete();
        if (!ret) {
            Log.e(TAG, "Failed to delete: %s", currentFile);
        }
        return ret;
    }

    /**
     * Get file size. If it is a directory, recursively get the size of all files within it.
     *
     * @param file The file or directory.
     * @return The size in bytes.
     */
    public static long getFileSizeBytes(File file) {
        if (file == null) return 0L;
        if (file.isDirectory()) {
            long size = 0L;
            final File[] files = file.listFiles();
            if (files == null) {
                return size;
            }
            for (File f : files) {
                size += getFileSizeBytes(f);
            }
            return size;
        } else {
            return file.length();
        }
    }

    /** Performs a simple copy of inputStream to outputStream. */
    public static void copyStream(InputStream inputStream, OutputStream outputStream)
            throws IOException {
        byte[] buffer = new byte[8192];
        int amountRead;
        while ((amountRead = inputStream.read(buffer)) != -1) {
            outputStream.write(buffer, 0, amountRead);
        }
    }

    /**
     * Atomically copies the data from an input stream into an output file.
     * @param is Input file stream to read data from.
     * @param outFile Output file path.
     * @throws IOException in case of I/O error.
     */
    public static void copyStreamToFile(InputStream is, File outFile) throws IOException {
        File tmpOutputFile = new File(outFile.getPath() + ".tmp");
        try (OutputStream os = new FileOutputStream(tmpOutputFile)) {
            Log.i(TAG, "Writing to %s", outFile);
            copyStream(is, os);
        }
        if (!tmpOutputFile.renameTo(outFile)) {
            throw new IOException();
        }
    }

    /** Reads inputStream into a byte array. */
    public static byte[] readStream(InputStream inputStream) throws IOException {
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        FileUtils.copyStream(inputStream, data);
        return data.toByteArray();
    }

    /**
     * Returns a URI that points at the file.
     *
     * @param file File to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    public static Uri getUriForFile(File file) {
        // TODO(crbug.com/40514633): Uncomment this when http://crbug.com/709584 has been fixed.
        // assert !ThreadUtils.runningOnUiThread();
        Uri uri = null;

        try {
            // Try to obtain a content:// URI, which is preferred to a file:// URI so that
            // receiving apps don't attempt to determine the file's mime type (which often fails).
            uri = FileProviderUtils.getContentUriFromFile(file);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Could not create content uri: " + e);
        }

        if (uri == null) uri = Uri.fromFile(file);

        return uri;
    }

    /**
     * Returns the file extension, or an empty string if none.
     * @param file Name of the file, with or without the full path (Unix style).
     * @return empty string if no extension, extension otherwise.
     */
    public static String getExtension(String file) {
        int lastSep = file.lastIndexOf('/');
        int lastDot = file.lastIndexOf('.');
        if (lastSep >= lastDot) return ""; // Subsumes |lastDot == -1|.
        return file.substring(lastDot + 1).toLowerCase(Locale.US);
    }

    /** Queries and decodes bitmap from content provider. */
    public static @Nullable Bitmap queryBitmapFromContentProvider(Context context, Uri uri) {
        try (ParcelFileDescriptor parcelFileDescriptor =
                context.getContentResolver().openFileDescriptor(uri, "r")) {
            if (parcelFileDescriptor == null) {
                Log.w(TAG, "Null ParcelFileDescriptor from uri " + uri);
                return null;
            }
            FileDescriptor fileDescriptor = parcelFileDescriptor.getFileDescriptor();
            if (fileDescriptor == null) {
                Log.w(TAG, "Null FileDescriptor from uri " + uri);
                return null;
            }
            Bitmap bitmap = BitmapFactory.decodeFileDescriptor(fileDescriptor);
            if (bitmap == null) {
                Log.w(TAG, "Failed to decode image from uri " + uri);
                return null;
            }
            return bitmap;
        } catch (IOException e) {
            Log.w(TAG, "IO exception when reading uri " + uri);
        }
        return null;
    }

    /**
     * Copies a file from app-private storage into the public Downloads collection.
     *
     * @param sourceFilePath Absolute path to the source file.
     * @return The public content URI string on success, or null on failure.
     */
    @CalledByNative
    public static @JniType("std::optional<std::string>") @Nullable String
            copyFileToDownloadsCollection(
                    @JniType("std::string") String sourceFilePath,
                    @JniType("std::string") String mimeType) {
        File sourceFile = new File(sourceFilePath);
        if (!sourceFile.exists()) {
            Log.e(TAG, "Source file does not exist: " + sourceFilePath);
            return null;
        }

        String fileName = sourceFile.getName();
        Context context = ContextUtils.getApplicationContext();
        ContentResolver resolver = context.getContentResolver();

        final long now = TimeUtils.currentTimeMillis() / 1000;
        ContentValues values = new ContentValues();
        values.put(MediaColumns.TITLE, fileName);
        values.put(MediaColumns.DISPLAY_NAME, fileName);
        values.put(MediaColumns.MIME_TYPE, mimeType);
        values.put(MediaColumns.DATE_ADDED, now);
        values.put(MediaColumns.DATE_MODIFIED, now);
        values.put(MediaColumns.IS_PENDING, 1);
        values.put(MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);

        Uri collectionUri = Downloads.EXTERNAL_CONTENT_URI;
        Uri itemUri = resolver.insert(collectionUri, values);
        if (itemUri == null) {
            Log.e(TAG, "Failed to insert item into MediaStore");
            return null;
        }

        try (InputStream in = new FileInputStream(sourceFile);
                OutputStream out = resolver.openOutputStream(itemUri)) {
            if (out == null) {
                Log.e(TAG, "Failed to open output stream for MediaStore Uri");
                return null;
            }
            copyStream(in, out);
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy file to MediaStore: " + e.getMessage(), e);
            resolver.delete(itemUri, null, null);
            return null;
        }

        ContentValues updateValues = new ContentValues();
        updateValues.put(MediaColumns.IS_PENDING, 0);
        try {
            if (resolver.update(itemUri, updateValues, null, null) == 1) {
                return itemUri.toString();
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to update pending status: " + e.getMessage(), e);
        }
        resolver.delete(itemUri, null, null);
        return null;
    }

    /**
     * Gets the canonicalised absolute pathname for |filePath|. Returns empty string if the path is
     * invalid. This function can result in I/O so it can be slow.
     *
     * @param filePath Path of the file, has to be a file path instead of a content URI.
     * @return canonicalised absolute pathname for |filePath|.
     */
    public static String getAbsoluteFilePath(String filePath) {
        return FileUtilsJni.get().getAbsoluteFilePath(filePath);
    }

    @NativeMethods
    public interface Natives {
        /** Returns the canonicalised absolute pathname for |filePath|. */
        @JniType("std::string")
        String getAbsoluteFilePath(@JniType("std::string") String filePath);
    }
}
