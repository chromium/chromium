// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.function.Function;

/** Helper methods for dealing with Files. */
@JNINamespace("base::android")
public class FileUtils {
    private static final String TAG = "FileUtils";

    public static Function<String, Boolean> DELETE_ALL = filepath -> true;

    /**
     * Delete the given File and (if it's a directory) everything within it.
     * @param currentFile The file or directory to delete. Does not need to exist.
     * @param canDelete the {@link Function} function used to check if the file can be deleted.
     * @return True if the files are deleted, or files reserved by |canDelete|, false if failed to
     *         delete files.
     * @note Caveat: Return values from recursive deletes are ignored.
     * @note Caveat: |canDelete| is not robust; see https://crbug.com/1066733.
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
    @NonNull
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
    @Nullable
    public static Bitmap queryBitmapFromContentProvider(Context context, Uri uri) {
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
     * Gets the canonicalised absolute pathname for |filePath|. Returns empty string if the path is
     * invalid. This function can result in I/O so it can be slow.
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
