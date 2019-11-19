// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Queue used for running tasks, initiated through WebView APIs, on the UI thread.
 * The queue won't start running tasks until WebView has been initialized properly.
 */
public class WebViewChromiumRunQueue {
    private final Queue<Runnable> mQueue;
    private final ChromiumHasStartedCallable mChromiumHasStartedCallable;

    /**
     * Callable representing whether WebView has been initialized, and we should start running
     * tasks.
     */
    public static interface ChromiumHasStartedCallable { public boolean hasStarted(); }

    public WebViewChromiumRunQueue(ChromiumHasStartedCallable chromiumHasStartedCallable) {
        mQueue = new ConcurrentLinkedQueue<Runnable>();
        mChromiumHasStartedCallable = chromiumHasStartedCallable;
    }

    /**
     * Add a new task to the queue. If WebView has already been initialized the task will be run
     * ASAP.
     */
    public void addTask(Runnable task) {
        mQueue.add(task);
        if (mChromiumHasStartedCallable.hasStarted()) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> { drainQueue(); });
        }
    }

    /**
     * Drain the queue, i.e. perform all the tasks in the queue.
     */
    public void drainQueue() {
        if (mQueue == null || mQueue.isEmpty()) {
            return;
        }

        Runnable task = mQueue.poll();
        while (task != null) {
            task.run();
            task = mQueue.poll();
        }
    }

    public boolean chromiumHasStarted() {
        return mChromiumHasStartedCallable.hasStarted();
    }

    public <T> T runBlockingFuture(FutureTask<T> task) {
        if (!chromiumHasStarted()) throw new RuntimeException("Must be started before we block!");
        if (ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("This method should only be called off the UI thread");
        }
        addTask(task);
        try {
            return task.get(4, TimeUnit.SECONDS);
        } catch (java.util.concurrent.TimeoutException e) {
            throw new RuntimeException("Probable deadlock detected due to WebView API being called "
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
}
