// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Environment;
import android.system.Os;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class provides the path related methods for the native library.
 */
@MainDex
public abstract class PathUtils {
    private static final String TAG = "PathUtils";
    private static final String THUMBNAIL_DIRECTORY_NAME = "textures";

    private static final int DATA_DIRECTORY = 0;
    private static final int THUMBNAIL_DIRECTORY = 1;
    private static final int CACHE_DIRECTORY = 2;
    private static final int NUM_DIRECTORIES = 3;
    private static final AtomicBoolean sInitializationStarted = new AtomicBoolean();
    private static FutureTask<String[]> sDirPathFetchTask;

    // If the FutureTask started in setPrivateDataDirectorySuffix() fails to complete by the time we
    // need the values, we will need the suffix so that we can restart the task synchronously on
    // the UI thread.
    private static String sDataDirectorySuffix;
    private static String sCacheSubDirectory;

    // Prevent instantiation.
    private PathUtils() {}

    /**
     * Initialization-on-demand holder. This exists for thread-safe lazy initialization. It will
     * cause getOrComputeDirectoryPaths() to be called (safely) the first time DIRECTORY_PATHS is
     * accessed.
     *
     * <p>See https://en.wikipedia.org/wiki/Initialization-on-demand_holder_idiom.
     */
    private static class Holder {
        private static final String[] DIRECTORY_PATHS = getOrComputeDirectoryPaths();
    }

    /**
     * Get the directory paths from sDirPathFetchTask if available, or compute it synchronously
     * on the UI thread otherwise. This should only be called as part of Holder's initialization
     * above to guarantee thread-safety as part of the initialization-on-demand holder idiom.
     */
    private static String[] getOrComputeDirectoryPaths() {
        try {
            // We need to call sDirPathFetchTask.cancel() here to prevent races. If it returns
            // true, that means that the task got canceled successfully (and thus, it did not
            // finish running its task). Otherwise, it failed to cancel, meaning that it was
            // already finished.
            if (sDirPathFetchTask.cancel(false)) {
                // Allow disk access here because we have no other choice.
                try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                    // sDirPathFetchTask did not complete. We have to run the code it was supposed
                    // to be responsible for synchronously on the UI thread.
                    return PathUtils.setPrivateDataDirectorySuffixInternal();
                }
            } else {
                // sDirPathFetchTask succeeded, and the values we need should be ready to access
                // synchronously in its internal future.
                return sDirPathFetchTask.get();
            }
        } catch (InterruptedException e) {
        } catch (ExecutionException e) {
        }

        return null;
    }

    @SuppressLint("NewApi")
    private static void chmod(String path, int mode) {
        // Both Os.chmod and ErrnoException require SDK >= 21. But while Dalvik on < 21 tolerates
        // Os.chmod, it throws VerifyError for ErrnoException, so catch Exception instead.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;
        try {
            Os.chmod(path, mode);
        } catch (Exception e) {
            Log.e(TAG, "Failed to set permissions for path \"" + path + "\"");
        }
    }

    /**
     * Fetch the path of the directory where private data is to be stored by the application. This
     * is meant to be called in an FutureTask in setPrivateDataDirectorySuffix(), but if we need the
     * result before the FutureTask has had a chance to finish, then it's best to cancel the task
     * and run it on the UI thread instead, inside getOrComputeDirectoryPaths().
     *
     * @see Context#getDir(String, int)
     */
    private static String[] setPrivateDataDirectorySuffixInternal() {
        String[] paths = new String[NUM_DIRECTORIES];
        Context appContext = ContextUtils.getApplicationContext();
        paths[DATA_DIRECTORY] = appContext.getDir(
                sDataDirectorySuffix, Context.MODE_PRIVATE).getPath();
        // MODE_PRIVATE results in rwxrwx--x, but we want rwx------, as a defence-in-depth measure.
        chmod(paths[DATA_DIRECTORY], 0700);
        paths[THUMBNAIL_DIRECTORY] = appContext.getDir(
                THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE).getPath();
        if (appContext.getCacheDir() != null) {
            if (sCacheSubDirectory == null) {
                paths[CACHE_DIRECTORY] = appContext.getCacheDir().getPath();
            } else {
                paths[CACHE_DIRECTORY] =
                        new File(appContext.getCacheDir(), sCacheSubDirectory).getPath();
            }
        }
        return paths;
    }

    /**
     * Starts an asynchronous task to fetch the path of the directory where private data is to be
     * stored by the application.
     *
     * <p>This task can run long (or more likely be delayed in a large task queue), in which case we
     * want to cancel it and run on the UI thread instead. Unfortunately, this means keeping a bit
     * of extra static state - we need to store the suffix and the application context in case we
     * need to try to re-execute later.
     *
     * @param suffix The private data directory suffix.
     * @param cacheSubDir The subdirectory in the cache directory to use, if non-null.
     * @see Context#getDir(String, int)
     */
    public static void setPrivateDataDirectorySuffix(String suffix, String cacheSubDir) {
        // This method should only be called once, but many tests end up calling it multiple times,
        // so adding a guard here.
        if (!sInitializationStarted.getAndSet(true)) {
            assert ContextUtils.getApplicationContext() != null;
            sDataDirectorySuffix = suffix;
            sCacheSubDirectory = cacheSubDir;

            // We don't use an AsyncTask because this function is called in early Webview startup
            // and it won't always have a UI thread available. Thus, we can't use AsyncTask which
            // inherently posts to the UI thread for onPostExecute().
            sDirPathFetchTask = new FutureTask<>(PathUtils::setPrivateDataDirectorySuffixInternal);
            AsyncTask.THREAD_POOL_EXECUTOR.execute(sDirPathFetchTask);
        } else {
            assert TextUtils.equals(sDataDirectorySuffix, suffix)
                : String.format("%s != %s", suffix, sDataDirectorySuffix);
            assert TextUtils.equals(sCacheSubDirectory, cacheSubDir)
                : String.format("%s != %s", cacheSubDir, sCacheSubDirectory);
        }
    }

    public static void setPrivateDataDirectorySuffix(String suffix) {
        setPrivateDataDirectorySuffix(suffix, null);
    }

    /**
     * @param index The index of the cached directory path.
     * @return The directory path requested.
     */
    private static String getDirectoryPath(int index) {
        return Holder.DIRECTORY_PATHS[index];
    }

    /**
     * @return the private directory that is used to store application data.
     */
    @CalledByNative
    public static String getDataDirectory() {
        assert sDirPathFetchTask != null : "setDataDirectorySuffix must be called first.";
        return getDirectoryPath(DATA_DIRECTORY);
    }

    /**
     * @return the cache directory.
     */
    @CalledByNative
    public static String getCacheDirectory() {
        assert sDirPathFetchTask != null : "setDataDirectorySuffix must be called first.";
        return getDirectoryPath(CACHE_DIRECTORY);
    }

    @CalledByNative
    public static String getThumbnailCacheDirectory() {
        assert sDirPathFetchTask != null : "setDataDirectorySuffix must be called first.";
        return getDirectoryPath(THUMBNAIL_DIRECTORY);
    }

    /**
     * @return the public downloads directory.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static String getDownloadsDirectory() {
        // TODO(crbug.com/508615): Temporarily allowing disk access until more permanent fix is in.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (BuildInfo.isAtLeastQ()) {
                // https://developer.android.com/preview/privacy/scoped-storage
                // In Q+, Android has bugun sandboxing external storage. Chrome may not have
                // permission to write to Environment.getExternalStoragePublicDirectory(). Instead
                // using Context.getExternalFilesDir() will return a path to sandboxed external
                // storage for which no additional permissions are required.
                return getAllPrivateDownloadsDirectories()[0];
            }
            return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                    .getPath();
        }
    }

    /**
     * @return Download directories including the default storage directory on SD card, and a
     * private directory on external SD card.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    public static String[] getAllPrivateDownloadsDirectories() {
        File[] files;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                files = ContextUtils.getApplicationContext().getExternalFilesDirs(
                        Environment.DIRECTORY_DOWNLOADS);
            }
        } else {
            files = new File[] {Environment.getExternalStorageDirectory()};
        }

        ArrayList<String> absolutePaths = new ArrayList<String>();
        for (int i = 0; i < files.length; ++i) {
            if (files[i] == null || TextUtils.isEmpty(files[i].getAbsolutePath())) continue;
            absolutePaths.add(files[i].getAbsolutePath());
        }

        return absolutePaths.toArray(new String[absolutePaths.size()]);
    }

    /**
     * @return the path to native libraries.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static String getNativeLibraryDirectory() {
        ApplicationInfo ai = ContextUtils.getApplicationContext().getApplicationInfo();
        if ((ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0
                || (ai.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
            return ai.nativeLibraryDir;
        }

        return "/system/lib/";
    }

    /**
     * @return the external storage directory.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    public static String getExternalStorageDirectory() {
        return Environment.getExternalStorageDirectory().getAbsolutePath();
    }
}
