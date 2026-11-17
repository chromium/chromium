// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.util.Log;

import dalvik.system.BaseDexClassLoader;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Creates ClassLoader for .dex file in a remote Context's APK. Non static for the sake of tests.
 */
@NullMarked
public class DexLoader {
    private static final int BUFFER_SIZE = 16 * 1024;
    private static final String TAG = "cr.DexLoader";

    /** Delete the given File and (if it's a directory) everything within it. */
    public static void deletePath(File file) {
        if (file == null) return;

        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deletePath(child);
                }
            }
        }

        if (!file.delete()) {
            Log.e(TAG, "Failed to delete : " + file.getAbsolutePath());
        }
    }

    /**
     * Creates ClassLoader for .dex file in {@link remoteContext}'s APK.
     *
     * @param remoteContext The context with the APK with the .dex file.
     * @param dexName The name of the .dex file in the APK.
     * @param canaryClassName Name of class in the .dex file. Used for testing the ClassLoader
     *     before returning it.
     * @param localDexDir Writable directory for caching data to speed up future calls to {@link
     *     #load()}.
     * @return The ClassLoader. Returns null on an error.
     */
    public @Nullable ClassLoader load(
            Context remoteContext, String dexName, String canaryClassName, File localDexDir) {
        File localDexFile = new File(localDexDir, dexName);
        boolean needsExtraction = !localDexFile.exists() || localDexFile.length() == 0;

        // Android 14+ refuses to load a .dex file which the app can write to. WebAPK shells
        // older than version 196 extracted writable .dex files, and these files survive WebAPK
        // updates. Make such a file read-only. If that fails, extract the file again.
        if (!needsExtraction && localDexFile.canWrite() && !localDexFile.setReadOnly()) {
            Log.w(TAG, "Could not make " + localDexFile.getPath() + " read-only");
            deletePath(localDexFile);
            needsExtraction = true;
        }

        // Extract the .dex file from the remote context's APK. Create a ClassLoader from the
        // extracted file.
        if (needsExtraction) {
            if (!localDexDir.exists() && !localDexDir.mkdirs()) {
                return null;
            }

            if (!extractAsset(remoteContext, dexName, localDexDir, localDexFile)) {
                Log.w(TAG, "Could not extract dex from assets");
                return null;
            }
        }

        File localOptimizedDir = new File(localDexDir, "optimized");
        if (!localOptimizedDir.exists() && !localOptimizedDir.mkdirs()) {
            return null;
        }

        return tryCreatingClassLoader(canaryClassName, localDexFile, localOptimizedDir);
    }

    /**
     * Deletes any files cached by {@link #load()}.
     *
     * @param localDexDir Cache directory passed to {@link #load()}.
     */
    public void deleteCachedDexes(File localDexDir) {
        deletePath(localDexDir);
    }

    /**
     * Extracts an asset from {@link context}'s APK to a read-only file.
     *
     * <p>Writes the asset to a temporary file in {@code destDir}, makes the temporary file
     * read-only, and then renames it to {@code destFile}. Android 14+ refuses to load a .dex file
     * which the app can write to. The rename replaces {@code destFile} if it exists, even if it is
     * read-only, because rename(2) checks the write permission of the directory, not of the file.
     *
     * @param assetName Name of the asset to extract.
     * @param destDir Directory which contains {@code destFile}. The temporary file is created in
     *     this directory, so that the rename stays on one file system.
     * @param destFile File to extract the asset to.
     * @return true on success.
     */
    private static boolean extractAsset(
            Context context, String assetName, File destDir, File destFile) {
        File tempFile;
        try {
            tempFile = File.createTempFile(destFile.getName(), ".tmp", destDir);
        } catch (IOException e) {
            Log.w(TAG, "Could not create temporary file in " + destDir.getPath(), e);
            return false;
        }

        try (InputStream inputStream = context.getAssets().open(assetName);
                OutputStream outputStream = new FileOutputStream(tempFile)) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int count;
            while ((count = inputStream.read(buffer, 0, BUFFER_SIZE)) != -1) {
                outputStream.write(buffer, 0, count);
            }
        } catch (IOException e) {
            Log.w(TAG, "Could not write " + tempFile.getPath(), e);
            deletePath(tempFile);
            return false;
        }

        if (!tempFile.setReadOnly()) {
            Log.w(TAG, "Could not make " + tempFile.getPath() + " read-only");
            deletePath(tempFile);
            return false;
        }

        if (!tempFile.renameTo(destFile)) {
            Log.w(TAG, "Could not rename " + tempFile.getPath() + " to " + destFile.getPath());
            deletePath(tempFile);
            return false;
        }
        return true;
    }

    /**
     * Tries to create ClassLoader with the given .dex file and optimized dex directory.
     *
     * @param canaryClassName Name of class in the .dex file. Used for testing the ClassLoader
     *     before returning it.
     * @param dexFile .dex file to create ClassLoader for.
     * @param optimizedDir Directory for storing the optimized dex file.
     * @return The ClassLoader. Returns null on an error.
     */
    private static @Nullable ClassLoader tryCreatingClassLoader(
            String canaryClassName, File dexFile, File optimizedDir) {
        try {
            ClassLoader loader =
                    new BaseDexClassLoader(
                            dexFile.getPath(),
                            optimizedDir,
                            null,
                            ClassLoader.getSystemClassLoader());
            // Loading {@link canaryClassName} will throw an exception if the .dex file cannot be
            // loaded.
            loader.loadClass(canaryClassName);
            return loader;
        } catch (Exception e) {
            String optimizedDirPath = (optimizedDir == null) ? null : optimizedDir.getPath();
            Log.w(
                    TAG,
                    "Could not load dex from "
                            + dexFile.getPath()
                            + " with optimized directory "
                            + optimizedDirPath);
            e.printStackTrace();
            return null;
        }
    }
}
