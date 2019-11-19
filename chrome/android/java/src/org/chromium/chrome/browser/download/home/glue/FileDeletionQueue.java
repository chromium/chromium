// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.FileUtils;
import org.chromium.base.task.AsyncTask;

import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

/**
 * Helper queue that will erase {@link File}s passed into {@link FileDeletionQueue#delete(String)}
 * or
 * {@link FileDeletionQueue#delete(List)} one at a time on an {@link AsyncTask#THREAD_POOL_EXECUTOR}
 * thread.  The deletions happen serially in order to prevent overloading the background thread
 * pool.
 */
class FileDeletionQueue {
    private final Queue<String> mFilePaths = new LinkedList<String>();

    /** The outstanding {@link AsyncTask} if any is currently running. */
    private FileDeletionTask mTask;

    /** {@link Callback} meant to be called on the background thread to perform the deletion. */
    private final Callback<String> mDeleter;

    /** @return A singleton instance of {@link FileDeletionQueue}. */
    public static FileDeletionQueue get() {
        return LazyHolder.INSTANCE;
    }

    /** Deletes {@code filePath} on a background thread at some point in the near future. */
    public void delete(String filePath) {
        mFilePaths.add(filePath);
        deleteNextFile();
    }

    /**
     * Deletes the files in {@code filePaths} on a background thread at some point in the near
     * future.
     */
    public void delete(List<String> filePaths) {
        mFilePaths.addAll(filePaths);
        deleteNextFile();
    }

    /**
     * @param deleter A {@link Callback} that will be triggered on the background thread to do the
     *                actual deleting of the file.
     */
    @VisibleForTesting
    FileDeletionQueue(Callback<String> deleter) {
        mDeleter = deleter;
    }

    private void deleteNextFile() {
        if (mTask != null) return;

        String filePath = mFilePaths.poll();
        if (filePath == null) return;

        mTask = new FileDeletionTask(filePath);
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private class FileDeletionTask extends AsyncTask<Void> {
        private final String mFilePath;

        FileDeletionTask(String filePath) {
            mFilePath = filePath;
        }

        @Override
        protected Void doInBackground() {
            mDeleter.onResult(mFilePath);
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            mTask = null;
            deleteNextFile();
        }
    }

    private static class LazyHolder {
        private static final FileDeletionQueue INSTANCE = new FileDeletionQueue(
                fileName -> FileUtils.batchDeleteFiles(CollectionUtil.newArrayList(fileName)));
    }
}
