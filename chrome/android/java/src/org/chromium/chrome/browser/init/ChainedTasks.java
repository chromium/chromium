// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.LinkedList;

/**
 * Allows to chain multiple tasks on the UI thread, with the next task posted when one completes.
 *
 * Threading:
 * - Can construct and call {@link add()} on any thread. Note that this is not synchronized.
 * - Can call {@link start()} on any thread, blocks if called from the UI thread and coalesceTasks
 *   is true.
 * - {@link cancel()} must be called from the UI thread.
 */
public class ChainedTasks {
    private LinkedList<Pair<TaskTraits, Runnable>> mTasks = new LinkedList<>();
    private volatile boolean mFinalized;

    private final Runnable mRunAndPost = new Runnable() {
        @Override
        public void run() {
            if (mTasks.isEmpty()) return;
            Pair<TaskTraits, Runnable> pair = mTasks.pop();
            pair.second.run();
            if (!mTasks.isEmpty()) PostTask.postTask(mTasks.peek().first, this);
        }
    };

    /**
     * Adds a task to the list of tasks to run. Cannot be called once {@link start()} has been
     * called.
     */
    public void add(TaskTraits traits, Runnable task) {
        if (mFinalized) throw new IllegalStateException("Must not call add() after start()");
        mTasks.add(new Pair<>(traits, task));
    }

    /**
     * Cancels the remaining tasks. Must be called from the UI thread.
     */
    public void cancel() {
        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("Must call cancel() from the UI thread.");
        }
        mFinalized = true;
        mTasks.clear();
    }

    /**
     * Posts or runs all the tasks, one by one.
     *
     * @param coalesceTasks if false, posts the tasks. Otherwise run them in a single task. If
     * called on the UI thread, run in the current task.
     */
    public void start(final boolean coalesceTasks) {
        if (mFinalized) throw new IllegalStateException("Cannot call start() several times");
        mFinalized = true;
        if (mTasks.isEmpty()) return;
        if (coalesceTasks) {
            PostTask.runOrPostTask(mTasks.peek().first, new Runnable() {
                @Override
                public void run() {
                    for (Pair<TaskTraits, Runnable> pair : mTasks) pair.second.run();
                    mTasks.clear();
                }
            });
        } else {
            PostTask.postTask(mTasks.peek().first, mRunAndPost);
        }
    }
}
