// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

import java.util.LinkedList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Handler for application level tasks to be completed on deferred startup. */
public class DeferredStartupHandler {
    private static DeferredStartupHandler sInstance;

    private final Queue<Runnable> mDeferredTasks = new LinkedList<>();

    private CountDownLatch mLatchForTesting;

    /**
     * This class is an application specific object that handles the deferred startup.
     * @return The singleton instance of {@link DeferredStartupHandler}.
     */
    public static DeferredStartupHandler getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new DeferredStartupHandler();
        return sInstance;
    }

    public static void setInstanceForTests(DeferredStartupHandler handler) {
        var oldValue = sInstance;
        sInstance = handler;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    protected DeferredStartupHandler() {}

    /**
     * Add the idle handler which will run deferred startup tasks in sequence when idle. This can
     * be called multiple times by different activities to schedule their own deferred startup
     * tasks.
     */
    public void queueDeferredTasksOnIdleHandler() {
        ThreadUtils.assertOnUiThread();
        // Adding multiple IdleHandlers is okay - they'll remove themselves once the queue is empty.
        Looper.myQueue()
                .addIdleHandler(
                        () -> {
                            try {
                                Runnable currentTask = mDeferredTasks.poll();
                                if (currentTask != null) currentTask.run();
                                if (mDeferredTasks.isEmpty()) {
                                    if (mLatchForTesting != null) mLatchForTesting.countDown();
                                    if (sInstance == DeferredStartupHandler.this) sInstance = null;
                                    return false;
                                }
                            } catch (Throwable e) {
                                // The Android MessageQueue swallows and logs all thrown exceptions
                                // leading to silently broken deferred startup handlers. Post the
                                // exception to avoid Android swallowing it.
                                new Handler()
                                        .post(
                                                () -> {
                                                    throw e;
                                                });
                            }
                            // Pump the queue so we get called back if the queue is still idle.
                            // Note that we can't simply check myQueue().isIdle() as this will
                            // continue to return true even if native tasks are queued up (until
                            // we return control to the Looper).
                            new Handler().post(CallbackUtils.emptyRunnable());
                            return true;
                        });
    }

    /**
     * Adds a single deferred task to the queue. The caller is responsible for calling
     * queueDeferredTasksOnIdleHandler after adding tasks.
     *
     * @param deferredTask The task to be run.
     */
    public void addDeferredTask(Runnable deferredTask) {
        ThreadUtils.assertOnUiThread();
        mDeferredTasks.add(deferredTask);
    }

    /**
     * Adds multiple deferred tasks to the queue. The caller is responsible for calling
     * queueDeferredTasksOnIdleHandler after adding tasks.
     *
     * @param deferredTasks The tasks to be run.
     */
    public void addDeferredTasks(List<Runnable> deferredTasks) {
        ThreadUtils.assertOnUiThread();
        mDeferredTasks.addAll(deferredTasks);
    }

    /**
     * Avoid using CriteriaHelper for waiting for deferred tasks to complete, as the act of polling
     * can prevent the Looper from going idle, preventing the tasks from running.
     *
     * <p>You should wait until the activity has posted its deferred startup tasks before calling
     * this function to avoid races.
     *
     * @return Whether deferred startup has been completed before the timeout expires.
     */
    public static boolean waitForDeferredStartupCompleteForTesting(long timeoutMillis) {
        ThreadUtils.assertOnBackgroundThread();
        // sInstance could become null while executing this function, so keep a ref here.
        DeferredStartupHandler instance =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            if (sInstance != null) {
                                sInstance.mLatchForTesting = new CountDownLatch(1);
                            }
                            return sInstance;
                        });
        // Tasks completed and instance was cleared before we started waiting.
        if (instance == null) return true;
        assert instance.mLatchForTesting != null;
        try {
            return instance.mLatchForTesting.await(timeoutMillis, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }
}
