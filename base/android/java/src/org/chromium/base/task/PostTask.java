// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;

/**
 * Java interface to the native chromium scheduler.  Note tasks can be posted before native
 * initialization, but task prioritization is extremely limited. Once the native scheduler
 * is ready, tasks will be migrated over.
 */
@JNINamespace("base")
public class PostTask {
    private static final Object sLock = new Object();
    private static Set<TaskRunner> sPreNativeTaskRunners =
            Collections.newSetFromMap(new WeakHashMap<TaskRunner, Boolean>());
    private static final Executor sPrenativeThreadPoolExecutor = new ChromeThreadPoolExecutor();
    private static Executor sPrenativeThreadPoolExecutorOverride;
    private static final TaskExecutor sTaskExecutors[] = getInitialTaskExecutors();
    private static boolean sNativeSchedulerReady;

    private static TaskExecutor[] getInitialTaskExecutors() {
        TaskExecutor taskExecutors[] = new TaskExecutor[TaskTraits.MAX_EXTENSION_ID + 1];
        taskExecutors[0] = new DefaultTaskExecutor();
        return taskExecutors;
    }

    /**
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static TaskRunner createTaskRunner(TaskTraits taskTraits) {
        synchronized (sLock) {
            return getTaskExecutorForTraits(taskTraits).createTaskRunner(taskTraits);
        }
    }

    /**
     * Creates and returns a SequencedTaskRunner. SequencedTaskRunners automatically destroy
     * themselves, so the destroy() function is not required to be called.
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        synchronized (sLock) {
            return getTaskExecutorForTraits(taskTraits).createSequencedTaskRunner(taskTraits);
        }
    }

    /**
     *
     * @param traits The TaskTraits that describe the desired TaskRunner.
     * @return The TaskRunner for the specified TaskTraits.
     */
    public static SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        synchronized (sLock) {
            return getTaskExecutorForTraits(taskTraits).createSingleThreadTaskRunner(taskTraits);
        }
    }

    /**
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void postTask(TaskTraits taskTraits, Runnable task) {
        postDelayedTask(taskTraits, task, 0);
    }

    /**
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     * @param delay The delay in milliseconds before the task can be run.
     */
    public static void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        synchronized (sLock) {
            if (sPreNativeTaskRunners != null || taskTraits.mIsChoreographerFrame) {
                getTaskExecutorForTraits(taskTraits).postDelayedTask(taskTraits, task, delay);
            } else {
                PostTaskJni.get().postDelayedTask(taskTraits.mPrioritySetExplicitly,
                        taskTraits.mPriority, taskTraits.mMayBlock, taskTraits.mUseThreadPool,
                        taskTraits.mUseCurrentThread, taskTraits.mExtensionId,
                        taskTraits.mExtensionData, task, delay);
            }
        }
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it.
     *
     * It should be executed only for tasks with traits corresponding to
     * executors backed by a SingleThreadTaskRunner, like UiThreadTaskTraits.
     *
     * Use this only for trivial tasks as it ignores task priorities.
     *
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    public static void runOrPostTask(TaskTraits taskTraits, Runnable task) {
        if (getTaskExecutorForTraits(taskTraits).canRunTaskImmediately(taskTraits)) {
            task.run();
        } else {
            postTask(taskTraits, task);
        }
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it and blocks until the task finishes.
     *
     * It should be executed only for tasks with traits corresponding to
     * executors backed by a SingleThreadTaskRunner, like UiThreadTaskTraits.
     *
     * Use this only for trivial tasks as it ignores task priorities.
     *
     * @deprecated In tests, use {@link
     *         org.chromium.content_public.browser.test.util.TestThreadUtils#runOnUiThreadBlocking(Runnable)
     *         TestThreadUtils.runOnUiThreadBlocking(Runnable)} instead. Non-test usage is heavily
     *         discouraged. For non-tests, use callbacks rather than blocking threads. If you
     * absolutely must block the thread, use FutureTask.get().
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     * @return The result of the callable
     */
    @Deprecated
    public static <T> T runSynchronously(TaskTraits taskTraits, Callable<T> c) {
        return runSynchronouslyInternal(taskTraits, new FutureTask<T>(c));
    }

    /**
     * This function executes the task immediately if the current thread is the
     * same as the one corresponding to the SingleThreadTaskRunner, otherwise it
     * posts it and blocks until the task finishes.
     *
     * It should be executed only for tasks with traits corresponding to
     * executors backed by a SingleThreadTaskRunner, like UiThreadTaskTraits.
     *
     * Use this only for trivial tasks as it ignores task priorities.
     *
     * @deprecated In tests, use {@link
     *         org.chromium.content_public.browser.test.util.TestThreadUtils#runOnUiThreadBlocking(Runnable)
     *         TestThreadUtils.runOnUiThreadBlocking(Runnable)} instead. Non-test usage is heavily
     *         discouraged. For non-tests, use callbacks rather than blocking threads. If you
     * absolutely must block the thread, use FutureTask.get().
     * @param taskTraits The TaskTraits that describe the desired TaskRunner.
     * @param task The task to be run with the specified traits.
     */
    @Deprecated
    public static void runSynchronously(TaskTraits taskTraits, Runnable r) {
        runSynchronouslyInternal(taskTraits, new FutureTask<Void>(r, null));
    }

    private static <T> T runSynchronouslyInternal(TaskTraits taskTraits, FutureTask<T> task) {
        runOrPostTask(taskTraits, task);
        try {
            return task.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Registers a TaskExecutor, this must be called before any other usages of this API.
     *
     * @param extensionId The id associated with the TaskExecutor.
     * @param taskExecutor The TaskExecutor to be registered. Must not equal zero.
     */
    public static void registerTaskExecutor(int extensionId, TaskExecutor taskExecutor) {
        synchronized (sLock) {
            assert extensionId != 0;
            assert extensionId <= TaskTraits.MAX_EXTENSION_ID;
            assert sTaskExecutors[extensionId] == null;
            sTaskExecutors[extensionId] = taskExecutor;
        }
    }

    /**
     * Lets a test override the pre-native thread pool executor.
     *
     * @param executor The Executor to use for pre-native thread pool tasks.
     */
    public static void setPrenativeThreadPoolExecutorForTesting(Executor executor) {
        synchronized (sLock) {
            sPrenativeThreadPoolExecutorOverride = executor;
        }
    }

    /**
     * Clears an override set by setPrenativeThreadPoolExecutorOverrideForTesting.
     */
    public static void resetPrenativeThreadPoolExecutorForTesting() {
        synchronized (sLock) {
            sPrenativeThreadPoolExecutorOverride = null;
        }
    }

    /**
     * @return The current Executor that PrenativeThreadPool tasks should run on.
     */
    static Executor getPrenativeThreadPoolExecutor() {
        synchronized (sLock) {
            if (sPrenativeThreadPoolExecutorOverride != null) {
                return sPrenativeThreadPoolExecutorOverride;
            }
            return sPrenativeThreadPoolExecutor;
        }
    }

    /**
     * Called by every TaskRunner on its creation, attempts to register this
     * TaskRunner as pre-native, unless the native scheduler has been
     * initialised already, and informs the caller about the outcome. Called
     * only when sLock has already been acquired.
     *
     * @param taskRunner The TaskRunner to be registered.
     * @return If the taskRunner got registered as pre-native.
     */
    static boolean registerPreNativeTaskRunnerLocked(TaskRunner taskRunner) {
        if (sPreNativeTaskRunners != null) {
            sPreNativeTaskRunners.add(taskRunner);
            return true;
        }
        return false;
    }

    private static TaskExecutor getTaskExecutorForTraits(TaskTraits traits) {
        return sTaskExecutors[traits.mExtensionId];
    }

    /**
     * @return True if the native scheduler is ready.
     */
    static boolean getNativeSchedulerReady() {
        synchronized (sLock) {
            return sNativeSchedulerReady;
        }
    }

    @CalledByNative
    private static void onNativeSchedulerReady() {
        synchronized (sLock) {
            Set<TaskRunner> preNativeTaskRunners = sPreNativeTaskRunners;
            sPreNativeTaskRunners = null;
            sNativeSchedulerReady = true;
            for (TaskRunner taskRunner : preNativeTaskRunners) {
                taskRunner.initNativeTaskRunner();
            }
        }
    }

    // This is here to make C++ tests work.
    @CalledByNative
    private static void onNativeSchedulerShutdown() {
        synchronized (sLock) {
            sPreNativeTaskRunners =
                    Collections.newSetFromMap(new WeakHashMap<TaskRunner, Boolean>());
            sNativeSchedulerReady = false;
        }
    }

    @NativeMethods
    interface Natives {
        void postDelayedTask(boolean prioritySetExplicitly, int priority, boolean mayBlock,
                boolean useThreadPool, boolean useCurrentThread, byte extensionId,
                byte[] extensionData, Runnable task, long delay);
    }
}
