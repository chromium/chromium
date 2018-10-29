// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.os.SystemClock;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.metrics.WebApkUma;

import java.io.File;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Manages directories created to store data for web apps.
 *
 * Directories managed by this class are all subdirectories of the app_WebappActivity/ directory,
 * which each WebappActivity using a directory named either for its Webapp's ID in Document mode,
 * or the index of the WebappActivity if it is a subclass of the WebappManagedActivity class (which
 * are used in pre-L devices to allow multiple WebappActivities launching).
 *
 * Also records metrics about files in the "WebAPK update" directory.
 */
public class WebappDirectoryManager {
    protected static final String WEBAPP_DIRECTORY_NAME = "WebappActivity";
    private static final String TAG = "WebappDirectoryManag";

    /** Path of subdirectory within cache directory which contains data for pending updates. */
    private static final String UPDATE_DIRECTORY_PATH = "webapk/update";

    /** Whether or not the class has already started trying to clean up obsolete directories. */
    private static final AtomicBoolean sMustCleanUpOldDirectories = new AtomicBoolean(true);

    /** AsyncTask that is used to clean up the web app directories. */
    private AsyncTask<Void> mCleanupTask;

    /**
     * Deletes web app directories with stale data.
     *
     * This should be called by a {@link WebappActivity} after it has restored all the data it
     * needs from its directory because the directory will be deleted during the process.
     *
     * @param context         Context to pull info and Files from.
     * @param currentWebappId ID for the currently running web app.
     * @return                AsyncTask doing the cleaning.
     */
    public void cleanUpDirectories(final Context context, final String currentWebappId) {
        if (mCleanupTask != null) return;

        mCleanupTask = new AsyncTask<Void>() {
            @Override
            protected final Void doInBackground() {
                recordNumberOfStaleWebApkUpdateRequestFiles();

                Set<File> directoriesToDelete = new HashSet<File>();
                directoriesToDelete.add(getWebappDirectory(context, currentWebappId));

                boolean shouldDeleteOldDirectories =
                        Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
                if (shouldDeleteOldDirectories && sMustCleanUpOldDirectories.getAndSet(false)) {
                    findStaleWebappDirectories(context, directoriesToDelete);
                }

                for (File directory : directoriesToDelete) {
                    if (isCancelled()) return null;
                    FileUtils.recursivelyDeleteFile(directory);
                }

                return null;
            }
        };
        mCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Cancels the cleanup task, if one exists. */
    public void cancelCleanup() {
        if (mCleanupTask != null) mCleanupTask.cancel(true);
    }

    /** Resets class' static state */
    public void resetForTesting() {
        sMustCleanUpOldDirectories.getAndSet(true);
    }

    /**
     * Finds all directories for web apps containing stale data.
     *
     * This includes all directories using the old pre-L directory structure, which used directories
     * named * app_WebappActivity*, as well as directories corresponding to WebappActivities that
     * are no longer listed in Android's recents, since these will be unable to restore their data.
     *
     * @param directoriesToDelete Set to append directory names to.
     */
    private void findStaleWebappDirectories(Context context, Set<File> directoriesToDelete) {
        File webappBaseDirectory = getBaseWebappDirectory(context);

        // Figure out what WebappActivities are still listed in Android's recents menu.
        Set<String> liveWebapps = new HashSet<String>();
        Set<Intent> baseIntents = getBaseIntentsForAllTasks();
        for (Intent intent : baseIntents) {
            Uri data = intent.getData();
            if (data != null && TextUtils.equals(WebappActivity.WEBAPP_SCHEME, data.getScheme())) {
                liveWebapps.add(data.getHost());
            }
        }

        // Delete all web app directories in the main directory, which were for pre-L web apps.
        File appDirectory = new File(context.getApplicationInfo().dataDir);
        String webappDirectoryAppBaseName = webappBaseDirectory.getName();
        File[] files = appDirectory.listFiles();
        if (files != null) {
            for (File file : files) {
                String filename = file.getName();
                if (!filename.startsWith(webappDirectoryAppBaseName)) continue;
                if (filename.length() == webappDirectoryAppBaseName.length()) continue;
                directoriesToDelete.add(file);
            }
        }

        // Clean out web app directories no longer corresponding to tasks in Recents.
        files = webappBaseDirectory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (!liveWebapps.contains(file.getName())) directoriesToDelete.add(file);
            }
        }
    }

    /** Records to UMA the count of old "WebAPK update request" files. */
    private void recordNumberOfStaleWebApkUpdateRequestFiles() {
        File updateDirectory = getWebApkUpdateDirectory();
        int count = 0;
        File[] children = updateDirectory.listFiles();
        if (children != null) {
            for (File child : children) {
                WebappDataStorage storage =
                        WebappRegistry.getInstance().getWebappDataStorage(child.getName());
                if (storage == null) {
                    ++count;
                    continue;
                }

                if (!storage.wasCheckForUpdatesDoneInLastMs(TimeUnit.DAYS.toMillis(1L))) {
                    ++count;
                }
            }
        }

        WebApkUma.recordNumberOfStaleWebApkUpdateRequestFiles(count);
    }

    /**
     * Returns the directory for a web app, creating it if necessary.
     * @param webappId ID for the web app.  Used as a subdirectory name.
     * @return File for storing information about the web app.
     */
    File getWebappDirectory(Context context, String webappId) {
        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/525781
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            long time = SystemClock.elapsedRealtime();
            File webappDirectory = new File(getBaseWebappDirectory(context), webappId);
            if (!webappDirectory.exists() && !webappDirectory.mkdir()) {
                Log.e(TAG, "Failed to create web app directory.");
            }
            RecordHistogram.recordTimesHistogram("Android.StrictMode.WebappDir",
                    SystemClock.elapsedRealtime() - time, TimeUnit.MILLISECONDS);
            return webappDirectory;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /** Returns the directory containing all of Chrome's web app data, creating it if needed. */
    final File getBaseWebappDirectory(Context context) {
        return context.getDir(WEBAPP_DIRECTORY_NAME, Context.MODE_PRIVATE);
    }

    /** Returns the directory for "WebAPK update" files. Does not create the directory. */
    static final File getWebApkUpdateDirectory() {
        return new File(PathUtils.getCacheDirectory(), UPDATE_DIRECTORY_PATH);
    }

    /** Returns the path for the "WebAPK update" file for the given {@link WebappDataStorage}. */
    static final File getWebApkUpdateFilePathForStorage(WebappDataStorage storage) {
        return new File(getWebApkUpdateDirectory(), storage.getId());
    }

    /** Returns a Set of Intents for all Chrome tasks currently known by the ActivityManager. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    protected Set<Intent> getBaseIntentsForAllTasks() {
        Set<Intent> baseIntents = new HashSet<Intent>();

        Context context = ContextUtils.getApplicationContext();
        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (AppTask task : manager.getAppTasks()) {
            Intent intent = DocumentUtils.getBaseIntentFromTask(task);
            if (intent != null) baseIntents.add(intent);
        }

        return baseIntents;
    }
}
