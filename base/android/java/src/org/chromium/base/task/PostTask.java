// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.JavaUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

import javax.annotation.concurrent.GuardedBy;

/**
 * Java interface to the native chromium scheduler. Note tasks can be posted before native
 * initialization, but task prioritization is extremely limited. Once the native scheduler is ready,
 * tasks will be migrated over.
 */
@JNINamespace("base")
public class PostTask {
    private static final String TAG = "PostTask";
    static final boolean ENABLE_TASK_ORIGINS = BuildConfig.ENABLE_ASSERTS;
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
    private static final ThreadLocal<TaskOriginException> sTaskOrigin =
            ENABLE_TASK_ORIGINS ? new ThreadLocal<>() : null;
    private static final TaskRunner[] sTraitsToRunnerMap =
            new TaskRunner[TaskTraits.UI_TRAITS_END + 1];

    static {
        for (@TaskTraits int i = 0; i <= TaskTraits.THREAD_POOL_TRAITS_END; i++) {
            sTraitsToRunnerMap[i] = new TaskRunnerImpl(i);
        }
        for (@TaskTraits int i = TaskTraits.UI_TRAITS_START; i <= TaskTraits.UI_TRAITS_END; i++) {
            sTraitsToRunnerMap[i] = new UiThreadTaskRunnerImpl(i);
        }
    }

    // Used by AsyncTask / ChainedTask to auto-cancel tasks from prior tests.
    static int sTestIterationForTesting;

    private static class TaskOriginRunnable implements Runnable {
        private final @Nullable TaskOriginException mTaskOrigin;
        private final Runnable mWrappedRunnable;

        TaskOriginRunnable(@Nullable TaskOriginException taskOrigin, Runnable wrappedRunnable) {
            this.mTaskOrigin = taskOrigin;
            this.mWrappedRunnable = wrappedRunnable;
        }

        @Override
        public void run() {
            sTaskOrigin.set(mTaskOrigin);
            try {
                mWrappedRunnable.run();
            } catch (Throwable t) {
                JavaUtils.throwUnchecked(maybeAddTaskOrigin(t));
            } finally {
                sTaskOrigin.remove();
            }
        }
    }

    private static boolean isUiTaskTraits(@TaskTraits int taskTraits) {
        return taskTraits >= TaskTraits.UI_TRAITS_START;
    }

    /**
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static TaskRunner createTaskRunner(@TaskTraits int taskTraits) {
        if (isUiTaskTraits(taskTraits)) {
            return sTraitsToRunnerMap[taskTraits];
        }
        return new TaskRunnerImpl(taskTraits);
    }

    /**
     * Creates and returns a SequencedTaskRunner. SequencedTaskRunners automatically destroy
     * themselves, so the destroy() function is not required to be called.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static SequencedTaskRunner createSequencedTaskRunner(@TaskTraits int taskTraits) {
        if (isUiTaskTraits(taskTraits)) {
            return (SequencedTaskRunner) sTraitsToRunnerMap[taskTraits];
        }
        return new SequencedTaskRunnerImpl(taskTraits);
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
        sTraitsToRunnerMap[taskTraits].postDelayedTask(task, delay);
    }

    /**
     * This function executes the task immediately if given UI task traits, and the current thread
     * is the UI thread.
     *
     * <p>Use this only for trivial tasks as it ignores task priorities.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void runOrPostTask(@TaskTraits int taskTraits, Runnable task) {
        if (canRunTaskImmediately(taskTraits)) {
            task.run();
        } else {
            postTask(taskTraits, task);
        }
    }

    /** Returns true if the traits are UI traits, and the current thread is the UI thread. */
    public static boolean canRunTaskImmediately(@TaskTraits int taskTraits) {
        if (isUiTaskTraits(taskTraits)) {
            return ThreadUtils.runningOnUiThread();
        }
        return false;
    }

    /**
     * Executes the task immediately if the current thread is the UI thread, otherwise it posts it
     * and blocks until the task finishes.
     *
     * <p>Usage outside of testing contexts is discouraged. Prefer callbacks in order to avoid
     * blocking.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param c The task to be run with the specified traits.
     * @return The result of the callable
     */
    public static <T> T runSynchronously(@TaskTraits int taskTraits, Callable<T> c) {
        return runSynchronouslyInternal(taskTraits, new FutureTask<T>(c));
    }

    /**
     * This function executes the task immediately if the current thread is the UI thread, otherwise
     * it posts it and blocks until the task finishes.
     *
     * <p>Usage outside of testing contexts is discouraged. Prefer callbacks in order to avoid
     * blocking.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param r The task to be run with the specified traits.
     */
    public static void runSynchronously(@TaskTraits int taskTraits, Runnable r) {
        runSynchronouslyInternal(taskTraits, new FutureTask<Void>(r, null));
    }

    private static <T> T runSynchronouslyInternal(@TaskTraits int taskTraits, FutureTask<T> task) {
        // Ensure no task origin "caused by" is added, since we are wrapping in a RuntimeException
        // anyways.
        Runnable r = ENABLE_TASK_ORIGINS ? populateTaskOrigin(null, task) : task;
        runOrPostTask(taskTraits, r);
        try {
            return task.get();
        } catch (Exception e) {
            // Use a RuntimeException rather than ExecutionException so that callers are not forced
            // to add try/catch.
            // Wrap the "caused by" rather than the ExecutionException to avoid excessive wrapping.
            throw new RuntimeException(e.getCause());
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

    public static @Nullable Exception getTaskOrigin() {
        return ENABLE_TASK_ORIGINS ? sTaskOrigin.get() : null;
    }

    /**
     * Adds the active TaskOriginException (if applicable) as the last "Caused By" to the given
     * exception.
     */
    public static <T extends Throwable> T maybeAddTaskOrigin(T exception) {
        Exception taskOrigin = getTaskOrigin();
        if (taskOrigin != null) {
            Throwable t = exception;
            while (t.getCause() != null) {
                t = t.getCause();
                assert !(t instanceof TaskOriginException)
                        : "Already wrapped: " + Log.getStackTraceString(exception);
            }
            try {
                t.initCause(taskOrigin);
            } catch (Exception e) {
                // Can happen if the cause of the exception was explicitly set to "null", which most
                // commonly happens for exceptions that have been deserialized.
            }
        }
        return exception;
    }

    /**
     * @param taskOrigin If null, ensures no origin will be added.
     */
    static Runnable populateTaskOrigin(
            @Nullable TaskOriginException taskOrigin, Runnable origTask) {
        // If runnable was wrapped higher up in the stack, then do not add another origin.
        if (origTask instanceof TaskOriginRunnable) {
            return origTask;
        }
        return new TaskOriginRunnable(taskOrigin, origTask);
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

    public static void resetUiThreadForTesting() {
        // UI Thread cannot be reset cleanly after native initialization.
        assert !sNativeInitialized;
    }
}
