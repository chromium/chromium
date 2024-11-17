// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.util.Log;

import dalvik.system.BaseDexClassLoader;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Creates ClassLoader for .dex file in a remote Context's APK. Non static for the sake of tests.
 */
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
    public ClassLoader load(
            Context remoteContext, String dexName, String canaryClassName, File localDexDir) {
        File localDexFile = new File(localDexDir, dexName);

        // Extract the .dex file from the remote context's APK. Create a ClassLoader from the
        // extracted file.
        if (!localDexFile.exists() || localDexFile.length() == 0) {
            if (!localDexDir.exists() && !localDexDir.mkdirs()) {
                return null;
            }

            if (!extractAsset(remoteContext, dexName, localDexFile)) {
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
     * Extracts an asset from {@link context}'s APK to a file.
     *
     * @param assetName Name of the asset to extract.
     * @param destFile File to extract the asset to.
     * @return true on success.
     */
    private static boolean extractAsset(Context context, String assetName, File destFile) {
        InputStream inputStream = null;
        OutputStream outputStream = null;
        try {
            inputStream = context.getAssets().open(assetName);
            outputStream = new FileOutputStream(destFile);
            byte[] buffer = new byte[BUFFER_SIZE];
            int count = 0;
            while ((count = inputStream.read(buffer, 0, BUFFER_SIZE)) != -1) {
                outputStream.write(buffer, 0, count);
            }
            inputStream.close();
            outputStream.close();
            return true;
        } catch (IOException e) {
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException ex) {
                }
            }
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException ex) {
                }
            }
        }
        return false;
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
    private static ClassLoader tryCreatingClassLoader(
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
