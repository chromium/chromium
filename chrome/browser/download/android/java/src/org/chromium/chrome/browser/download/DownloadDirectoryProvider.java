// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Environment;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.download.DirectoryOption.DownloadLocationDirectoryType;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Class to provide download related directory options including the default download directory on
 * the primary storage, or a private directory on external SD card.
 *
 * This class uses an asynchronous task to retrieve the directories, and guarantee only one task
 * can execute at any time. Multiple tasks may cause certain device fail to retrieve download
 * directories. Should be used on main thread.
 *
 * Also, this class listens to SD card insertion and removal events to update the directory
 * options accordingly.
 */
public class DownloadDirectoryProvider {
    private static final String TAG = "DownloadDirectory";

    /**
     * Delegate class to query directories from Android API. Should be created on main thread
     * and used on background thread in {@link AsyncTask}.
     */
    public interface Delegate {
        /**
         * Get the primary download directory. See {@link
         * DownloadDirectoryProvider#getPrimaryDownloadDirectory()}.
         */
        @NonNull
        File getPrimaryDownloadDirectory();

        /**
         * Get download directories on secondary storage.
         * @return A list of directories on the secondary storage.
         */
        @NonNull
        SecondaryStorageInfo getSecondaryStorageDownloadDirectories();
    }

    /** Class that calls Android API to get download directories. */
    public static class DownloadDirectoryProviderDelegate implements Delegate {
        @Override
        public File getPrimaryDownloadDirectory() {
            return DownloadDirectoryProvider.getPrimaryDownloadDirectory();
        }

        @Override
        public SecondaryStorageInfo getSecondaryStorageDownloadDirectories() {
            return DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories();
        }
    }

    /**
     * Asynchronous task to retrieve all download directories on a background thread. Only one task
     * can exist at the same time.
     */
    private class AllDirectoriesTask extends AsyncTask<ArrayList<DirectoryOption>> {
        private DownloadDirectoryProvider.Delegate mDelegate;

        AllDirectoriesTask(DownloadDirectoryProvider.Delegate delegate) {
            mDelegate = delegate;
        }

        @Override
        protected ArrayList<DirectoryOption> doInBackground() {
            ArrayList<DirectoryOption> dirs = new ArrayList<>();

            // Retrieve default directory.
            File defaultDirectory = mDelegate.getPrimaryDownloadDirectory();

            // If no default directory, return an error option.
            if (defaultDirectory == null) {
                dirs.add(
                        new DirectoryOption(
                                null, 0, 0, DirectoryOption.DownloadLocationDirectoryType.ERROR));
                return dirs;
            }

            DirectoryOption defaultOption =
                    toDirectoryOption(
                            defaultDirectory,
                            DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
            dirs.add(defaultOption);
            recordDirectoryType(DirectoryOption.DownloadLocationDirectoryType.DEFAULT);

            // Retrieve additional directories, i.e. the external SD card directory. This doesn't
            // include the legacy directories on Q+.
            mExternalStorageDirectory = Environment.getExternalStorageDirectory().getAbsolutePath();
            SecondaryStorageInfo secondaryStorageInfo =
                    mDelegate.getSecondaryStorageDownloadDirectories();
            List<File> secondaryDirs =
                    Build.VERSION.SDK_INT > Build.VERSION_CODES.Q
                            ? secondaryStorageInfo.directories
                            : secondaryStorageInfo.directoriesPreR;
            if (secondaryDirs.isEmpty()) return dirs;
            boolean hasAddtionalDirectory = false;
            for (File file : secondaryDirs) {
                if (file == null) continue;
                dirs.add(
                        toDirectoryOption(
                                file, DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL));
                hasAddtionalDirectory = true;
            }

            if (hasAddtionalDirectory) {
                recordDirectoryType(DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL);
            }

            return dirs;
        }

        @Override
        protected void onPostExecute(ArrayList<DirectoryOption> dirs) {
            mDirectoryOptions = dirs;
            mDirectoriesReady = true;
            mNeedsUpdate = false;

            for (Callback<ArrayList<DirectoryOption>> callback : mCallbacks) {
                callback.onResult(mDirectoryOptions);
            }

            mCallbacks.clear();
            mAllDirectoriesTask = null;
        }

        private DirectoryOption toDirectoryOption(
                File dir, @DownloadLocationDirectoryType int type) {
            if (dir == null) return null;
            return new DirectoryOption(
                    dir.getAbsolutePath(), dir.getUsableSpace(), dir.getTotalSpace(), type);
        }
    }

    // Singleton instance.
    private static class LazyHolder {
        private static DownloadDirectoryProvider sInstance = new DownloadDirectoryProvider();
    }

    /**
     * Get the instance of directory provider.
     * @return The singleton directory provider instance.
     */
    public static DownloadDirectoryProvider getInstance() {
        return LazyHolder.sInstance;
    }

    /**
     * Sets the directory provider for testing.
     * @param provider The directory provider used in tests.
     */
    public void setDirectoryProviderForTesting(DownloadDirectoryProvider provider) {
        var oldValue = LazyHolder.sInstance;
        LazyHolder.sInstance = provider;
        ResettersForTesting.register(() -> LazyHolder.sInstance = oldValue);
    }

    /** BroadcastReceiver to listen to external SD card insertion and removal events. */
    private final class ExternalSDCardReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Intent.ACTION_MEDIA_REMOVED)
                    || intent.getAction().equals(Intent.ACTION_MEDIA_MOUNTED)
                    || intent.getAction().equals(Intent.ACTION_MEDIA_EJECT)) {
                // When receiving SD card events, immediately retrieve download directory may not
                // yield correct result, mark needs update to force to fire another
                // AllDirectoriesTask on next getAllDirectoriesOptions call.
                mNeedsUpdate = true;
            }
        }
    }

    private ExternalSDCardReceiver mExternalSDCardReceiver;
    private boolean mDirectoriesReady;
    private boolean mNeedsUpdate;
    private AllDirectoriesTask mAllDirectoriesTask;
    private ArrayList<DirectoryOption> mDirectoryOptions;
    private String mExternalStorageDirectory;
    private ArrayList<Callback<ArrayList<DirectoryOption>>> mCallbacks = new ArrayList<>();

    protected DownloadDirectoryProvider() {
        registerSDCardReceiver();
    }

    /**
     * Get all available download directories.
     * @param callback The callback that carries the result of all download directories.
     */
    public void getAllDirectoriesOptions(Callback<ArrayList<DirectoryOption>> callback) {
        // Use cache value.
        if (!mNeedsUpdate && mDirectoriesReady) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback.bind(mDirectoryOptions));
            return;
        }

        mCallbacks.add(callback);
        updateDirectories();
    }

    /**
     * Retrieves the external storage directory from in-memory cache. On Android M,
     * {@link Environment#getExternalStorageDirectory} may access disk, so this operation can't be
     * done on main thread.
     * @return The external storage path or null if the or null if the asynchronous task to query
     * the directories is not finished.
     */
    public String getExternalStorageDirectory() {
        if (mDirectoriesReady) return mExternalStorageDirectory;
        return null;
    }

    /**
     * Get the primary download directory. Before Android Q, this is the public external download
     * directory. Starting from Android Q, this is the app private download directory on primary
     * storage.
     * The directory will be created if it doesn't exist. Should be called on background thread.
     * @return The download directory. Can be an invalid directory if failed to create the
     *         directory.
     */
    public static @Nullable File getPrimaryDownloadDirectory() {
        String primaryDownloadDir = PathUtils.getDownloadsDirectory();
        if (TextUtils.isEmpty(primaryDownloadDir)) return null;

        File downloadDir = new File(primaryDownloadDir);

        // Create the directory if needed.
        if (!downloadDir.exists()) {
            try {
                downloadDir.mkdirs();
            } catch (SecurityException e) {
                Log.e(TAG, "Exception when creating download directory.", e);
            }
        }
        return downloadDir;
    }

    /** Contains download directories on secondary storage(external SD card). */
    public static class SecondaryStorageInfo {
        /**
         * The download directories on secondary storage from Android R. Will be null before Android
         * R.
         */
        @Nullable public final List<File> directories;

        /**
         * The download directories on secondary storage pre R. Some downloads may exist in these
         * directories on Q+.
         */
        public final List<File> directoriesPreR;

        /**
         * Construct the secondary storage info.
         * @param directories See {@link #directories}.
         * @param directoriesPreR See {@link #directoriesPreR}.
         */
        public SecondaryStorageInfo(List<File> directories, List<File> directoriesPreR) {
            this.directories = directories;
            this.directoriesPreR = directoriesPreR;
        }
    }

    /**
     * Get download directories on secondary storage.
     * @return The {@link SecondaryStorageInfo} that contains the download directories on secondary
     * storages.
     */
    public static SecondaryStorageInfo getSecondaryStorageDownloadDirectories() {
        // Starting from Android R, we use a different location for secondary storage.
        ArrayList<File> directoriesPreR = new ArrayList<>();
        String[] dirPaths = PathUtils.getAllPrivateDownloadsDirectories();
        // The first element returned from getAllPrivateDownloadsDirectories() is on primary
        // storage.
        for (int i = 1; i < dirPaths.length; ++i) directoriesPreR.add(new File(dirPaths[i]));

        ArrayList<File> directoriesOnR = new ArrayList<>();
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.Q) {
            dirPaths = PathUtils.getExternalDownloadVolumesNames();
            // getExternalDownloadVolumesNames() doesn't include dirs on primary storage.
            for (String dir : dirPaths) directoriesOnR.add(new File(dir));
            return new SecondaryStorageInfo(directoriesOnR, directoriesPreR);
        }

        return new SecondaryStorageInfo(null, directoriesPreR);
    }

    /**
     * Returns whether the downloaded file path is on an external SD card.
     * @param filePath The download file path.
     */
    public static boolean isDownloadOnSDCard(String filePath) {
        if (ContentUriUtils.isContentUri(filePath) || filePath == null) return false;

        // Check private dirs on secondary storage. On Android R, there might be legacy downloads
        // that use this path before the migration to getExternalDownloadVolumesNames().
        String[] dirs = PathUtils.getAllPrivateDownloadsDirectories();
        for (int i = 1; i < dirs.length; ++i) {
            if (filePath.startsWith(dirs[i])) return true;
        }

        // Check directories returned from media volume API on R.
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            dirs = PathUtils.getExternalDownloadVolumesNames();
            for (String dir : dirs) {
                if (filePath.startsWith(dir)) return true;
            }
        }

        return false;
    }

    private void updateDirectories() {
        // If asynchronous task is pending, wait for its result.
        if (mAllDirectoriesTask != null) return;

        mAllDirectoriesTask = new AllDirectoriesTask(new DownloadDirectoryProviderDelegate());
        mAllDirectoriesTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void registerSDCardReceiver() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_MEDIA_REMOVED);
        filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
        filter.addAction(Intent.ACTION_MEDIA_EJECT);
        filter.addDataScheme("file");
        mExternalSDCardReceiver = new ExternalSDCardReceiver();
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mExternalSDCardReceiver, filter);
    }

    private void recordDirectoryType(@DirectoryOption.DownloadLocationDirectoryType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Location.DirectoryType",
                type,
                DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);
    }
}
