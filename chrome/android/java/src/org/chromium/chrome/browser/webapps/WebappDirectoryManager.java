// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;

import java.io.File;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Manages directories created to store data for WebAPK updates, and cleans up stale state
 * directories left behind under app_WebappActivity/.
 *
 * Also records metrics about files in the "WebAPK update" directory.
 */
public class WebappDirectoryManager {
    protected static final String DEPRECATED_WEBAPP_DIRECTORY_NAME = "WebappActivity";

    /** Path of subdirectory within cache directory which contains data for pending updates. */
    private static final String UPDATE_DIRECTORY_PATH = "webapk/update";

    /** Whether or not the class has already started trying to clean up obsolete directories. */
    private static final AtomicBoolean sMustCleanUpOldDirectories = new AtomicBoolean(true);

    /** Deletes web app directories with stale data. */
    public static void cleanUpDirectories() {
        if (!sMustCleanUpOldDirectories.getAndSet(false)) return;

        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected final Void doInBackground() {
                recordNumberOfStaleWebApkUpdateRequestFiles();
                FileUtils.recursivelyDeleteFile(
                        getBaseWebappDirectory(ContextUtils.getApplicationContext()),
                        FileUtils.DELETE_ALL);
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Resets class' static state */
    public static void resetForTesting() {
        sMustCleanUpOldDirectories.set(true);
    }

    /** Records to UMA the count of old "WebAPK update request" files. */
    private static void recordNumberOfStaleWebApkUpdateRequestFiles() {
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

                if (!storage.wasCheckForUpdatesDoneInLastMs(DateUtils.DAY_IN_MILLIS)) {
                    ++count;
                }
            }
        }

        WebApkUmaRecorder.recordNumberOfStaleWebApkUpdateRequestFiles(count);
    }

    /** Returns the directory containing all of Chrome's web app data, creating it if needed. */
    static final File getBaseWebappDirectory(Context context) {
        return context.getDir(DEPRECATED_WEBAPP_DIRECTORY_NAME, Context.MODE_PRIVATE);
    }

    /** Returns the directory for "WebAPK update" files. Does not create the directory. */
    static final File getWebApkUpdateDirectory() {
        return new File(PathUtils.getCacheDirectory(), UPDATE_DIRECTORY_PATH);
    }

    /** Returns the path for the "WebAPK update" file for the given {@link WebappDataStorage}. */
    static final File getWebApkUpdateFilePathForStorage(WebappDataStorage storage) {
        return new File(getWebApkUpdateDirectory(), storage.getId());
    }
}
