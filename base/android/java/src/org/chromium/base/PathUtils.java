// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.provider.MediaStore;
import android.system.Os;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;

import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicBoolean;

/** This class provides the path related methods for the native library. */
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
    private static String sDataDirectoryBasePath;
    private static String sCacheDirectoryBasePath;

    // Prevent instantiation.
    private PathUtils() {}

    // Resetting is useful in Robolectric tests, where each test is run with a different
    // data directory.
    public static void resetForTesting() {
        sInitializationStarted.set(false);
        sDirPathFetchTask = null;
        sDataDirectorySuffix = null;
        sCacheSubDirectory = null;
        sDataDirectoryBasePath = null;
        sCacheDirectoryBasePath = null;
    }

    /**
     * Get the directory paths from sDirPathFetchTask if available, or compute it synchronously
     * on the UI thread otherwise. This should only be called as part of Holder's initialization
     * above to guarantee thread-safety as part of the initialization-on-demand holder idiom.
     */
    private static String[] getOrComputeDirectoryPaths() {
        if (!sDirPathFetchTask.isDone()) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                // No-op if already ran.
                sDirPathFetchTask.run();
            }
        }
        try {
            return sDirPathFetchTask.get();
        } catch (Exception e) {
            throw JavaUtils.throwUnchecked(e);
        }
    }

    private static void chmod(String path, int mode) {
        try {
            Os.chmod(path, mode);
        } catch (Exception e) {
            Log.e(TAG, "Failed to set permissions for path \"" + path + "\"");
        }
    }

    // TODO(crbug.com/41484704): Merge the Chrome and WebView implementations
    // of isPathUnderAppDir into one.
    @RequiresApi(Build.VERSION_CODES.N)
    public static boolean isPathUnderAppDir(String path, Context context) {
        File file = new File(path);
        File dataDir = context.getDataDir();
        File externalDir = ContextUtils.getApplicationContext().getExternalFilesDir(null);
        try {
            return (file.toPath().toRealPath().startsWith(dataDir.toPath().toRealPath())
                    || file.toPath().toRealPath().startsWith(externalDir.toPath().toRealPath()));
        } catch (Exception e) {
            return false;
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
    private static String[] setPrivateDirectoryPathInternal() {
        String[] paths = new String[NUM_DIRECTORIES];
        File dataDir = null;
        File thumbnailDir = null;
        Context appContext = ContextUtils.getApplicationContext();
        if (sDataDirectoryBasePath == null) {
            dataDir = appContext.getDir(sDataDirectorySuffix, Context.MODE_PRIVATE);
            thumbnailDir = appContext.getDir(THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE);
        } else {
            dataDir = new File(sDataDirectoryBasePath, sDataDirectorySuffix);
            dataDir.mkdirs();
            thumbnailDir = new File(sDataDirectoryBasePath, THUMBNAIL_DIRECTORY_NAME);
            thumbnailDir.mkdirs();
        }

        File cacheDir = null;
        if (sCacheDirectoryBasePath != null) {
            cacheDir = new File(sCacheDirectoryBasePath);
        } else {
            cacheDir = appContext.getCacheDir();
        }
        if (cacheDir != null) {
            if (sCacheSubDirectory != null) {
                cacheDir = new File(cacheDir, sCacheSubDirectory);
            }
            if (sCacheDirectoryBasePath != null || sCacheSubDirectory != null) {
                cacheDir.mkdirs();
                // Set to rwx--S--- as the Android cache dir has a distinct gid and is setgid.
                chmod(cacheDir.getPath(), 02700);
            }
            paths[CACHE_DIRECTORY] = cacheDir.getPath();
        }
        paths[DATA_DIRECTORY] = dataDir.getPath();
        // MODE_PRIVATE results in rwxrwx--x, but we want rwx------, as a defence-in-depth measure.
        chmod(paths[DATA_DIRECTORY], 0700);
        paths[THUMBNAIL_DIRECTORY] = thumbnailDir.getPath();
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
     * @param dataBasePath The base path for the data directory. If null, defaults to using Android
     *          Platform specific app data directory.
     * @param cacheBasePath The base path for the cache directory. If null, defaults to using
     *          Android Platform specific app cache directory.
     * @param dataDirSuffix The private data directory suffix.
     * @param cacheSubDir The subdirectory in the cache directory to use, if non-null.
     * @see Context#getDir(String, int)
     */
    public static void setPrivateDirectoryPath(
            String dataBasePath, String cacheBasePath, String dataDirSuffix, String cacheSubDir) {
        // This method should only be called once, but many tests end up calling it multiple times,
        // so adding a guard here.
        if (!sInitializationStarted.getAndSet(true)) {
            assert ContextUtils.getApplicationContext() != null;
            sDataDirectoryBasePath = dataBasePath;
            sCacheDirectoryBasePath = cacheBasePath;
            sDataDirectorySuffix = dataDirSuffix;
            sCacheSubDirectory = cacheSubDir;

            // We don't use an AsyncTask because this function is called in early Webview startup
            // and it won't always have a UI thread available. Thus, we can't use AsyncTask which
            // inherently posts to the UI thread for onPostExecute().
            sDirPathFetchTask = new FutureTask<>(PathUtils::setPrivateDirectoryPathInternal);
            AsyncTask.THREAD_POOL_EXECUTOR.execute(sDirPathFetchTask);
        } else {
            assert TextUtils.equals(sDataDirectoryBasePath, dataBasePath)
                    : String.format("%s != %s", dataBasePath, sDataDirectoryBasePath);
            assert TextUtils.equals(sCacheDirectoryBasePath, cacheBasePath)
                    : String.format("%s != %s", cacheBasePath, sCacheDirectoryBasePath);
            assert TextUtils.equals(sDataDirectorySuffix, dataDirSuffix)
                    : String.format("%s != %s", dataDirSuffix, sDataDirectorySuffix);
            assert TextUtils.equals(sCacheSubDirectory, cacheSubDir)
                    : String.format("%s != %s", cacheSubDir, sCacheSubDirectory);
        }
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
        setPrivateDirectoryPath(null, null, suffix, cacheSubDir);
    }

    public static void setPrivateDataDirectorySuffix(String suffix) {
        setPrivateDataDirectorySuffix(suffix, null);
    }

    /**
     * @param index The index of the cached directory path.
     * @return The directory path requested.
     */
    private static String getDirectoryPath(int index) {
        String[] paths = getOrComputeDirectoryPaths();
        return paths[index];
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

    // Should not be called from WebView, since it does not support being used in a multiprocess
    // environment.
    @CalledByNative
    public static String getThumbnailCacheDirectory() {
        assert sDirPathFetchTask != null : "setDataDirectorySuffix must be called first.";
        return getDirectoryPath(THUMBNAIL_DIRECTORY);
    }

    /**
     * Returns the downloads directory. Before Android Q, this returns the public download directory
     * for Chrome app. On Q+, this returns the first private download directory for the app, since Q
     * will block public directory access. May return empty string when there are no external
     * storage volumes mounted.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    public static @NonNull String getDownloadsDirectory() {
        // TODO(crbug.com/41187555): Move calls to getDownloadsDirectory() to background thread.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // https://developer.android.com/preview/privacy/scoped-storage
                // In Q+, Android has begun sandboxing external storage. Chrome may not have
                // permission to write to Environment.getExternalStoragePublicDirectory(). Instead
                // using Context.getExternalFilesDir() will return a path to sandboxed external
                // storage for which no additional permissions are required.
                String[] dirs = getAllPrivateDownloadsDirectories();
                assert dirs != null;
                return dirs.length == 0 ? "" : dirs[0];
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
    public static @NonNull String[] getAllPrivateDownloadsDirectories() {
        List<File> files = new ArrayList<>();
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            File[] externalDirs =
                    ContextUtils.getApplicationContext()
                            .getExternalFilesDirs(Environment.DIRECTORY_DOWNLOADS);
            files = (externalDirs == null) ? files : Arrays.asList(externalDirs);
        }
        return toAbsolutePathStrings(files);
    }

    /**
     * @return The download directory for secondary storage on Q+, returned by {@link
     *     MediaStore#getExternalVolumeNames(Context)}. Notices on Android R, apps can no longer
     *     expose app's private directory for secondary storage. Apps should put files to
     *     /storage/$volume_id/Download/ directory instead.
     */
    @RequiresApi(Build.VERSION_CODES.R)
    @CalledByNative
    public static @NonNull String[] getExternalDownloadVolumesNames() {
        ArrayList<File> files = new ArrayList<>();
        Set<String> volumes =
                MediaStore.getExternalVolumeNames(ContextUtils.getApplicationContext());
        for (String vol : volumes) {
            if (!TextUtils.isEmpty(vol) && !vol.contains(MediaStore.VOLUME_EXTERNAL_PRIMARY)) {
                StorageManager manager =
                        ContextUtils.getApplicationContext().getSystemService(StorageManager.class);
                Uri uri = MediaStore.Files.getContentUri(vol);
                try {
                    File volumeDir = manager.getStorageVolume(uri).getDirectory();
                    File volumeDownloadDir = new File(volumeDir, Environment.DIRECTORY_DOWNLOADS);
                    // Happens in rare case when Android doesn't create the download directory for
                    // this volume.
                    if (!volumeDownloadDir.isDirectory()) {
                        Log.w(
                                TAG,
                                "Download dir missing: %s, parent dir:%s, isDirectory:%s",
                                volumeDownloadDir.getAbsolutePath(),
                                volumeDir.getAbsolutePath(),
                                volumeDir.isDirectory());
                    }
                    files.add(volumeDownloadDir);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to get storage volume for uri: " + uri, e);
                }
            }
        }

        return toAbsolutePathStrings(files);
    }

    private static @NonNull String[] toAbsolutePathStrings(@NonNull List<File> files) {
        ArrayList<String> absolutePaths = new ArrayList<String>();
        for (File file : files) {
            if (file == null || TextUtils.isEmpty(file.getAbsolutePath())) continue;
            absolutePaths.add(file.getAbsolutePath());
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
