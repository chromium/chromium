// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

import java.io.File;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Encapsulates the directory, {@link SharedPreferences}, {@link SequencedTaskRunner}, and clear
 * counter for a specific storage partition scope defined by an arbitrary string tag.
 *
 * <p>Each directory scope represents an isolated storage partition on disk (e.g. "active_tabs",
 * "actor_tabs_xyz") and manages serialized background file I/O operations without depending on
 * native Profile lifecycles.
 */
@NullMarked
/* package */ class TabCacheDirScope {
    private static final String TAG = "tab_cache";

    private final String mTag;
    private final File mCacheDirectory;
    private final SharedPreferences mSharedPreferences;
    private final SequencedTaskRunner mTaskRunner;
    private final AtomicInteger mClearCounter = new AtomicInteger(0);

    /**
     * @param tag The arbitrary tag used as the cache directory and SharedPreferences name.
     */
    /* package */ TabCacheDirScope(String tag) {
        mTag = tag;
        mCacheDirectory = ContextUtils.getApplicationContext().getDir(tag, Context.MODE_PRIVATE);
        mSharedPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(tag, Context.MODE_PRIVATE);
        mTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    }

    /** Returns the tag used for this directory scope. */
    /* package */ String getTag() {
        return mTag;
    }

    /** Returns the cache directory for this directory scope. */
    /* package */ File getCacheDirectory() {
        return mCacheDirectory;
    }

    /** Returns the cache directory, creating it if it does not exist. */
    /* package */ File getOrCreateCacheDirectory() {
        if (!mCacheDirectory.exists() && !mCacheDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create tab cache directory: " + mCacheDirectory);
        }
        return mCacheDirectory;
    }

    /** Returns the SharedPreferences associated with this scope. */
    /* package */ SharedPreferences getSharedPreferences() {
        return mSharedPreferences;
    }

    /** Returns the SequencedTaskRunner used for background operations in this scope. */
    /* package */ SequencedTaskRunner getTaskRunner() {
        return mTaskRunner;
    }

    /** Returns the clear counter used to invalidate pending background tasks. */
    /* package */ AtomicInteger getClearCounter() {
        return mClearCounter;
    }

    /** Clears all cached files and preferences in this directory scope. */
    /* package */ void clearAll() {
        mClearCounter.incrementAndGet();
        mSharedPreferences.edit().clear().apply();
        mTaskRunner.execute(this::clearDirectoryInternal);
    }

    private void clearDirectoryInternal() {
        if (mCacheDirectory.exists()) {
            FileUtils.recursivelyDeleteFile(mCacheDirectory, FileUtils.DELETE_ALL);
        }
    }

    /**
     * Deletes the given file and associated preference in this scope.
     *
     * @param fileName The name of the file and key in SharedPreferences.
     */
    /* package */ void deleteFileAndPref(String fileName) {
        mSharedPreferences.edit().remove(fileName).apply();
        int currClearCount = mClearCounter.get();
        mTaskRunner.execute(
                () -> {
                    if (currClearCount != mClearCounter.get()) return;

                    File file = new File(mCacheDirectory, fileName);
                    if (file.exists() && !file.delete()) {
                        Log.e(TAG, "Failed to delete cache file: " + file);
                    }
                });
    }
}
