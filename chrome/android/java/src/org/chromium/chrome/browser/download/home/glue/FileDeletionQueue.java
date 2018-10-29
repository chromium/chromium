// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import android.support.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

/**
 * Helper queue that will erase {@link File}s passed into {@link FileDeletionQueue#delete(File)} or
 * {@link FileDeletionQueue#delete(List)} one at a time on an {@link AsyncTask#THREAD_POOL_EXECUTOR}
 * thread.  The deletions happen serially in order to prevent overloading the background thread
 * pool.
 */
class FileDeletionQueue {
    private final Queue<File> mFiles = new LinkedList<File>();

    /** The outstanding {@link AsyncTask} if any is currently running. */
    private FileDeletionTask mTask;

    /** {@link Callback} meant to be called on the background thread to perform the deletion. */
    private final Callback<File> mDeleter;

    /** @return A singleton instance of {@link FileDeletionQueue}. */
    public static FileDeletionQueue get() {
        return LazyHolder.INSTANCE;
    }

    /** Deletes {@code file} on a background thread at some point in the near future. */
    public void delete(File file) {
        mFiles.add(file);
        deleteNextFile();
    }

    /**
     * Deletes the {@link File}s in {@code files} on a background thread at some point in the near
     * future.
     */
    public void delete(List<File> files) {
        mFiles.addAll(files);
        deleteNextFile();
    }

    /**
     * @param deleter A {@link Callback} that will be triggered on the background thread to do the
     *                actual deleting of the file.
     */
    @VisibleForTesting
    FileDeletionQueue(Callback<File> deleter) {
        mDeleter = deleter;
    }

    private void deleteNextFile() {
        if (mTask != null) return;

        File file = mFiles.poll();
        if (file == null) return;

        System.out.println("dtrainor: Starting " + file.getName());

        mTask = new FileDeletionTask(file);
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private class FileDeletionTask extends AsyncTask<Void> {
        private final File mFile;

        FileDeletionTask(File file) {
            mFile = file;
        }

        @Override
        protected Void doInBackground() {
            mDeleter.onResult(mFile);
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            super.onPostExecute(result);
            mTask = null;
            deleteNextFile();
        }
    }

    private static class LazyHolder {
        private static final FileDeletionQueue INSTANCE =
                new FileDeletionQueue(file -> FileUtils.recursivelyDeleteFile(file));
    }
}
