// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Handler;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.JavaUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

import javax.annotation.concurrent.GuardedBy;

/**
 * Java interface to the native chromium scheduler.  Note tasks can be posted before native
 * initialization, but task prioritization is extremely limited. Once the native scheduler
 * is ready, tasks will be migrated over.
 */
@JNINamespace("base")
public class PostTask {
    private static final String TAG = "PostTask";
    private static final Object sPreNativeTaskRunnerLock = new Object();

    @GuardedBy("sPreNativeTaskRunnerLock")
    private static List<TaskRunnerImpl> sPreNativeTaskRunners = new ArrayList<>();

    // Volatile is sufficient for synchronization here since we never need to read-write. This is a
    // one-way switch (outside of testing) and volatile makes writes to it immediately visible to
    // other threads.
    private static volatile boolean sNativeInitialized;
    private static ChromeThreadPoolExecutor sPrenativeThreadPoolExecutor =
            new ChromeThreadPoolExecutor();
    private static volatile Executor sPrenativeThreadPoolExecutorForTesting;

    private static final ThreadPoolTaskExecutor sThreadPoolTaskExecutor =
            new ThreadPoolTaskExecutor();
    // Initialized on demand or when the UI thread is initialized to allow embedders (eg WebView) to
    // override the UI thread.
    private static UiThreadTaskExecutor sUiThreadTaskExecutor;

    // Used by AsyncTask / ChainedTask to auto-cancel tasks from prior tests.
    static int sTestIterationForTesting;

    /**
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static TaskRunner createTaskRunner(@TaskTraits int taskTraits) {
        return getTaskExecutorForTraits(taskTraits).createTaskRunner(taskTraits);
    }

    /**
     * Creates and returns a SequencedTaskRunner. SequencedTaskRunners automatically destroy
     * themselves, so the destroy() function is not required to be called.
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static SequencedTaskRunner createSequencedTaskRunner(@TaskTraits int taskTraits) {
        return getTaskExecutorForTraits(taskTraits).createSequencedTaskRunner(taskTraits);
    }

    /**
     *
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static SingleThreadTaskRunner createSingleThreadTaskRunner(@TaskTraits int taskTraits) {
        return getTaskExecutorForTraits(taskTraits).createSingleThreadTaskRunner(taskTraits);
    }

    /**
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void postTask(@TaskTraits int taskTraits, Runnable task) {
        postDelayedTask(taskTraits, task, 0);
    }

    /**
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     * @param delay The delay in milliseconds before the task can be run.
     */
    public static void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
        getTaskExecutorForTraits(taskTraits).postDelayedTask(taskTraits, task, delay);
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it.
     *
     * It should be executed only for tasks with traits corresponding to
     * executors backed by a SingleThreadTaskRunner, like TaskTraits.UI_*.
     *
     * Use this only for trivial tasks as it ignores task priorities.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void runOrPostTask(@TaskTraits int taskTraits, Runnable task) {
        if (getTaskExecutorForTraits(taskTraits).canRunTaskImmediately(taskTraits)) {
            task.run();
        } else {
            postTask(taskTraits, task);
        }
    }

    /**
     * Returns true if the task can be executed immediately (i.e. the current thread is the same as
     * the one corresponding to the SingleThreadTaskRunner)
     */
    public static boolean canRunTaskImmediately(@TaskTraits int taskTraits) {
        return getTaskExecutorForTraits(taskTraits).canRunTaskImmediately(taskTraits);
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it and blocks until the task finishes.
     *
     * Usage outside of testing contexts is discouraged. Prefer callbacks in order
     * to avoid blocking.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     * @return The result of the callable
     */
    public static <T> T runSynchronously(@TaskTraits int taskTraits, Callable<T> c) {
        return runSynchronouslyInternal(taskTraits, new FutureTask<T>(c));
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it and blocks until the task finishes.
     *
     * Usage outside of testing contexts is discouraged. Prefer callbacks in order
     * to avoid blocking.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void runSynchronously(@TaskTraits int taskTraits, Runnable r) {
        runSynchronouslyInternal(taskTraits, new FutureTask<Void>(r, null));
    }

    private static <T> T runSynchronouslyInternal(@TaskTraits int taskTraits, FutureTask<T> task) {
        runOrPostTask(taskTraits, task);
        try {
            return task.get();
        } catch (Exception e) {
            throw JavaUtils.throwUnchecked(e);
        }
    }

    /**
     * Lets a test override the pre-native thread pool executor.
     *
     * @param executor The Executor to use for pre-native thread pool tasks.
     */
    public static void setPrenativeThreadPoolExecutorForTesting(Executor executor) {
        sPrenativeThreadPoolExecutorForTesting = executor;
        ResettersForTesting.register(() -> sPrenativeThreadPoolExecutorForTesting = null);
    }

    /**
     * @return The current Executor that PrenativeThreadPool tasks should run on.
     */
    static Executor getPrenativeThreadPoolExecutor() {
        if (sPrenativeThreadPoolExecutorForTesting != null) {
            return sPrenativeThreadPoolExecutorForTesting;
        }
        return sPrenativeThreadPoolExecutor;
    }

    /**
     * Called by every TaskRunnerImpl on its creation, attempts to register this TaskRunner as
     * pre-native, unless the native scheduler has been initialized already, and informs the caller
     * about the outcome.
     *
     * @param taskRunner The TaskRunnerImpl to be registered.
     * @return If the taskRunner got registered as pre-native.
     */
    static boolean registerPreNativeTaskRunner(TaskRunnerImpl taskRunner) {
        synchronized (sPreNativeTaskRunnerLock) {
            if (sPreNativeTaskRunners == null) return false;
            sPreNativeTaskRunners.add(taskRunner);
            return true;
        }
    }

    private static TaskExecutor getTaskExecutorForTraits(@TaskTraits int traits) {
        if (traits >= TaskTraits.UI_TRAITS_START) {
            // UI thread may be posted to before initialized, so trigger the initialization.
            if (sUiThreadTaskExecutor == null) ThreadUtils.getUiThreadHandler();
            return sUiThreadTaskExecutor;
        }
        return sThreadPoolTaskExecutor;
    }

    @CalledByNative
    private static void onNativeSchedulerReady() {
        // Unit tests call this multiple times.
        if (sNativeInitialized) return;
        sNativeInitialized = true;
        List<TaskRunnerImpl> preNativeTaskRunners;
        synchronized (sPreNativeTaskRunnerLock) {
            preNativeTaskRunners = sPreNativeTaskRunners;
            sPreNativeTaskRunners = null;
        }
        for (TaskRunnerImpl taskRunner : preNativeTaskRunners) {
            taskRunner.initNativeTaskRunner();
        }
    }

    /** Drops all queued pre-native tasks. */
    public static void flushJobsAndResetForTesting() throws InterruptedException {
        ChromeThreadPoolExecutor executor = sPrenativeThreadPoolExecutor;
        // Potential race condition, but by checking queue size first we overcount if anything.
        int taskCount = executor.getQueue().size() + executor.getActiveCount();
        if (taskCount > 0) {
            executor.shutdownNow();
            executor.awaitTermination(1, TimeUnit.SECONDS);
            sPrenativeThreadPoolExecutor = new ChromeThreadPoolExecutor();
        }
        synchronized (sPreNativeTaskRunnerLock) {
            // Clear rather than rely on sTestIterationForTesting in case there are task runners
            // that are stored in static fields (re-used between tests).
            if (sPreNativeTaskRunners != null) {
                for (TaskRunnerImpl taskRunner : sPreNativeTaskRunners) {
                    // Clearing would not reliably work in non-robolectric environments since
                    // a currently running background task could post a new task after the queue
                    // is cleared. However, Robolectric controls executors to prevent actual
                    // concurrency, so this approach should work fine.
                    taskCount += taskRunner.clearTaskQueueForTesting();
                }
            }
            sTestIterationForTesting++;
        }
        sPrenativeThreadPoolExecutorForTesting = null;
        if (taskCount > 0) {
            Log.w(TAG, "%d background task(s) existed after test finished.", taskCount);
        }
    }

    /** Called once when the UI thread has been initialized */
    public static void onUiThreadReady(Handler uiThreadHandler) {
        assert sUiThreadTaskExecutor == null;
        sUiThreadTaskExecutor = new UiThreadTaskExecutor(uiThreadHandler);
    }

    public static void resetUiThreadForTesting() {
        // UI Thread cannot be reset cleanly after native initialization.
        assert !sNativeInitialized;

        sUiThreadTaskExecutor = null;
    }
}
