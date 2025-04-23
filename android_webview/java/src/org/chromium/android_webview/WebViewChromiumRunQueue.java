// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Queue used for running tasks, initiated through WebView APIs, on the UI thread. The queue won't
 * start running tasks until WebView has been initialized properly.
 */
@Lifetime.Singleton
@NullMarked
public class WebViewChromiumRunQueue {
    private final Queue<Runnable> mQueue = new ConcurrentLinkedQueue<Runnable>();
    private volatile boolean mChromiumStarted;

    public WebViewChromiumRunQueue() {}

    /**
     * Add a new task to the queue. If the queue has already been drained the task will be run ASAP.
     */
    public void addTask(Runnable task) {
        mQueue.add(task);
        if (mChromiumStarted) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, this::drainQueue);
        }
    }

    /**
     * Mark that Chromium has started and drain the queue, i.e. perform all the tasks in the queue.
     */
    public void notifyChromiumStarted() {
        mChromiumStarted = true;
        drainQueue();
    }

    public <T> T runBlockingFuture(FutureTask<T> task) {
        if (!mChromiumStarted) throw new RuntimeException("Must be started before we block!");
        if (ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("This method should only be called off the UI thread");
        }
        addTask(task);
        try {
            return task.get(4, TimeUnit.SECONDS);
        } catch (java.util.concurrent.TimeoutException e) {
            throw new RuntimeException(
                    "Probable deadlock detected due to WebView API being called "
                            + "on incorrect thread while the UI thread is blocked.",
                    e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debugging
    // deadlocks.
    // Do not call this method while on the UI thread!
    public void runVoidTaskOnUiThreadBlocking(Runnable r) {
        FutureTask<Void> task = new FutureTask<Void>(r, null);
        runBlockingFuture(task);
    }

    public <T> T runOnUiThreadBlocking(Callable<T> c) {
        return runBlockingFuture(new FutureTask<T>(c));
    }

    private void drainQueue() {
        Runnable task = mQueue.poll();
        while (task != null) {
            task.run();
            task = mQueue.poll();
        }
    }
}
