// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.Log;

import dalvik.system.DexClassLoader;
import dalvik.system.DexFile;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * This class provides a method to optimize .dex files.
 * Note: This class is copied (mostly) verbatim from DexOptUtils in GMSCore.
 */
public class DexOptimizer {
    private static final String TAG = "DexOptimzer";

    private static final String DEX_SUFFIX = ".dex";
    private static final String ODEX_SUFFIX = ".odex";

    /**
     * Creates optimized odex file for the specified dex file.
     * @param dexFile Path to a dex file.
     * @return True if the dex file was successfully optimized.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("SetWorldReadable")
    public static boolean optimize(File dexFile) {
        if (!dexFile.exists()) {
            Log.e(TAG, "Dex file does not exist! " + dexFile.getAbsolutePath());
            return false;
        }

        try {
            if (!DexFile.isDexOptNeeded(dexFile.getAbsolutePath())) {
                return true;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to check optimization status: " + e.toString() + " : "
                            + e.getMessage());
        }

        File odexDir = null;
        try {
            odexDir = ensureOdexDirectory(dexFile);
        } catch (IOException e) {
            Log.e(TAG, "Failed to create odex directory! " + e.getMessage());
            return false;
        }

        File generatedDexDir = odexDir;
        if (generatedDexDir.equals(dexFile.getParentFile())) {
            generatedDexDir = new File(odexDir, "optimized");
            if (!generatedDexDir.exists() && !generatedDexDir.mkdirs()) {
                return false;
            }
        }

        new DexClassLoader(
                dexFile.getAbsolutePath(),
                generatedDexDir.getAbsolutePath(),
                null,
                ClassLoader.getSystemClassLoader());
        File optimizedFile = new File(generatedDexDir, dexFile.getName());
        if (!optimizedFile.exists()) {
            Log.e(TAG, "Failed to create dex.");
            return false;
        }

        File destOdexFile = new File(odexDir, replaceExtension(optimizedFile.getName(), "odex"));
        if (!optimizedFile.renameTo(destOdexFile)) {
            Log.e(TAG, "Failed to rename optimized file.");
            return false;
        }

        if (!destOdexFile.setReadable(true, false)) {
            Log.e(TAG, "Failed to make odex world readable.");
            return false;
        }

        return true;
    }

    /**
     * Guesses the directory that DexClassLoader looks in for the odex file based on the
     * Android OS version and the dex path.
     * @param dexPath
     * @return Guess for the default odex directory.
     */
    private static File odexDirectory(File dexPath) {
        int currentApiVersion = Build.VERSION.SDK_INT;
        try {
            if (currentApiVersion >= Build.VERSION_CODES.M) {
                return new File(
                        dexPath.getParentFile(), "oat/" + VMRuntime.getCurrentInstructionSet());
            } else if (currentApiVersion >= Build.VERSION_CODES.LOLLIPOP) {
                return new File(dexPath.getParentFile(), VMRuntime.getCurrentInstructionSet());
            } else {
                return dexPath.getParentFile();
            }
        } catch (NoSuchMethodException e) {
            return null;
        }
    }

    /**
     * Guesses the directory that DexClassLoader looks in for the odex file based on the
     * Android OS version and the dex path. Creates the directory if it does not exist.
     * @param dexPath
     * @return Guess for the default odex directory.
     */
    private static File ensureOdexDirectory(File dexPath) throws IOException {
        File odexDir = odexDirectory(dexPath);
        if (odexDir == null) {
            throw new IOException("Failed to create odex cache directory. "
                    + "Could not determine odex directory.");
        }
        if (!odexDir.exists()) {
            boolean success = odexDir.mkdirs();
            if (!success) {
                throw new IOException(
                        "Failed to create odex cache directory in data directory.");
            }
            // The full path to the odex must be traversable.
            File root = dexPath.getParentFile();
            File dir = odexDir;
            while (dir != null && !root.equals(dir)) {
                if (!dir.setExecutable(true, false)) {
                    throw new IOException("Failed to make odex directory world traversable: "
                            + dir.getAbsolutePath());
                }
                dir = dir.getParentFile();
            }
        }
        return odexDir;
    }

    /**
     * Replaces a file name's extension.
     *
     * @param name File name to modify.
     * @param extension New extension.
     * @return File name with new extension.
     */
    private static String replaceExtension(String name, String extension) {
        int lastDot = name.lastIndexOf(".");
        StringBuilder sb = new StringBuilder(lastDot + extension.length());
        sb.append(name, 0, lastDot + 1);
        sb.append(extension);
        return sb.toString();
    }

    /**
     * Makes use of a hidden API to retrieve the instruction set name for the currently
     * executing process. This string is used to form the directory name for the generated
     * odex.
     *
     * - This API is not available on pre-L devices, but as the pre-L runtime did not scope odex
     *   files by <isa> on pre-L, this is not a problem.
     *
     * - For devices L+, it's still possible for this API to be missing. In that case
     *   we will fallback to A) interpretation, and failing that B) generate an odex in the
     *   client's file space.
     */
    private static class VMRuntime {
        @SuppressLint("NewApi")
        @SuppressWarnings("unchecked")
        public static String getCurrentInstructionSet() throws NoSuchMethodException {
            Method getCurrentInstructionSetMethod;
            try {
                Class c = Class.forName("dalvik.system.VMRuntime");
                getCurrentInstructionSetMethod = c.getDeclaredMethod("getCurrentInstructionSet");
            } catch (ClassNotFoundException | NoSuchMethodException e) {
                Log.w(TAG, "dalvik.system.VMRuntime#getCurrentInstructionSet is unsupported.", e);
                throw new NoSuchMethodException(
                        "dalvik.system.VMRuntime#getCurrentInstructionSet could not be found.");
            }
            try {
                return (String) getCurrentInstructionSetMethod.invoke(null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                Log.w(TAG, "Failed to call dalvik.system.VMRuntime#getCurrentInstructionSet", e);
                throw new NoSuchMethodException(
                        "dalvik.system.VMRuntime#getCurrentInstructionSet could not be found.");
            }
        }
    }
}
