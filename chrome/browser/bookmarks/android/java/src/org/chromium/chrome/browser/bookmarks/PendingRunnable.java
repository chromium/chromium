// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/**
 * Simple interface to collapse multiple posts of the same runnable into a single deduplicated
 * execution.
 */
public class PendingRunnable {
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
    private final @TaskTraits int mTaskTraits;
    private final Runnable mRunnable;

    private boolean mIsPending;

    /**
     * @param taskTraits Traits to run with.
     * @param runnable The actual task to be executed.
     */
    public PendingRunnable(@TaskTraits int taskTraits, Runnable runnable) {
        mTaskTraits = taskTraits;
        mRunnable = runnable;
    }

    /** Posts a task to run the runnable this object holds. */
    public void post() {
        mThreadChecker.assertOnValidThread();
        if (!mIsPending) {
            mIsPending = true;
            PostTask.postTask(mTaskTraits, this::onRun);
        }
    }

    private void onRun() {
        mThreadChecker.assertOnValidThread();
        mIsPending = false;
        mRunnable.run();
    }
}
