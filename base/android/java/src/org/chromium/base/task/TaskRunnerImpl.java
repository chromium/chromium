// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Process;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import javax.annotation.concurrent.GuardedBy;

/**
 * Implementation of the abstract class {@link TaskRunnerImpl}. Uses AsyncTasks until
 * native APIs are available.
 */
@JNINamespace("base")
public class TaskRunnerImpl implements TaskRunner {
    // TaskRunnerCleaners are enqueued to this queue when their WeakReference to a TaskRunnerIml is
    // cleared.
    private static final ReferenceQueue<Object> sQueue = new ReferenceQueue<>();
    // Holds a strong reference to the pending TaskRunnerCleaners so they don't get GC'd before the
    // TaskRunnerImpl they're weakly referencing does.
    @GuardedBy("sCleaners")
    private static final Set<TaskRunnerCleaner> sCleaners = new HashSet<>();

    private final TaskTraits mTaskTraits;
    private final String mTraceEvent;
    private final @TaskRunnerType int mTaskRunnerType;
    // Volatile is sufficient for synchronization here since we never need to read-write and
    // volatile makes writes to it immediately visible to other threads.
    // When |mNativeTaskRunnerAndroid| is set, native has been initialized and pre-native tasks have
    // been migrated to the native task runner.
    private volatile long mNativeTaskRunnerAndroid;
    protected final Runnable mRunPreNativeTaskClosure = this::runPreNativeTask;

    private final Object mPreNativeTaskLock = new Object();
    @GuardedBy("mPreNativeTaskLock")
    private boolean mDidOneTimeInitialization;
    @Nullable
    @GuardedBy("mPreNativeTaskLock")
    private LinkedList<Runnable> mPreNativeTasks;
    @Nullable
    @GuardedBy("mPreNativeTaskLock")
    private List<Pair<Runnable, Long>> mPreNativeDelayedTasks;

    private static class TaskRunnerCleaner extends WeakReference<TaskRunnerImpl> {
        final long mNativePtr;

        TaskRunnerCleaner(TaskRunnerImpl runner) {
            super(runner, sQueue);
            mNativePtr = runner.mNativeTaskRunnerAndroid;
        }

        void destroy() {
            TaskRunnerImplJni.get().destroy(mNativePtr);
        }
    }

    /**
     * The lifecycle for a TaskRunner is very complicated. Some task runners are static and never
     * destroyed, some have a task posted to them and are immediately allowed to be GC'd by the
     * creator, but if native isn't initialized the task would be lost if this were to be GC'd.
     * This makes an explicit destroy impractical as it can't be enforced on static runners, and
     * wouldn't actually destroy the runner before native initialization as that would cause tasks
     * to be lost. A finalizer could give us the correct behaviour here, but finalizers are banned
     * due to the performance cost they impose, and all of the many correctness gotchas around
     * implementing a finalizer.
     *
     * The strategy we've gone with here is to use a ReferenceQueue to keep track of which
     * TaskRunners are no longer reachable (and may have been GC'd), and to delete the native
     * counterpart for those TaskRunners by polling the queue when doing non-performance-critical
     * operations on TaskRunners, like creating a new one. In order to prevent this TaskRunner from
     * being GC'd before its tasks can be posted to the native runner, PostTask holds a strong
     * reference to each TaskRunner with a task posted to it before native initialization.
     */
    private static void destroyGarbageCollectedTaskRunners() {
        while (true) {
            // ReferenceQueue#poll immediately removes and returns an element from the queue,
            // returning null if the queue is empty.
            @SuppressWarnings("unchecked")
            TaskRunnerCleaner cleaner = (TaskRunnerCleaner) sQueue.poll();
            if (cleaner == null) return;
            cleaner.destroy();
            synchronized (sCleaners) {
                sCleaners.remove(cleaner);
            }
        }
    }

    /**
     * @param traits The TaskTraits associated with this TaskRunnerImpl.
     */
    TaskRunnerImpl(TaskTraits traits) {
        this(traits, "TaskRunnerImpl", TaskRunnerType.BASE);
        destroyGarbageCollectedTaskRunners();
    }

    /**
     * @param traits The TaskTraits associated with this TaskRunnerImpl.
     * @param traceCategory Specifies the name of this instance's subclass for logging purposes.
     * @param taskRunnerType Specifies which subclass is this instance for initialising the correct
     *         native scheduler.
     */
    protected TaskRunnerImpl(
            TaskTraits traits, String traceCategory, @TaskRunnerType int taskRunnerType) {
        mTaskTraits = traits.withExplicitDestination();
        mTraceEvent = traceCategory + ".PreNativeTask.run";
        mTaskRunnerType = taskRunnerType;
    }

    @Override
    public void postTask(Runnable task) {
        postDelayedTask(task, 0);
    }

    @Override
    public void postDelayedTask(Runnable task, long delay) {
        // Lock-free path when native is initialized.
        if (mNativeTaskRunnerAndroid != 0) {
            TaskRunnerImplJni.get().postDelayedTask(
                    mNativeTaskRunnerAndroid, task, delay, task.getClass().getName());
            return;
        }
        synchronized (mPreNativeTaskLock) {
            oneTimeInitialization();
            if (mNativeTaskRunnerAndroid != 0) {
                TaskRunnerImplJni.get().postDelayedTask(
                        mNativeTaskRunnerAndroid, task, delay, task.getClass().getName());
                return;
            }
            // We don't expect a whole lot of these, if that changes consider pooling them.
            // If a task is scheduled for immediate execution, we post it on the
            // pre-native task runner. Tasks scheduled to run with a delay will
            // wait until the native task runner is initialised.
            if (delay == 0) {
                mPreNativeTasks.add(task);
                schedulePreNativeTask();
            } else {
                Pair<Runnable, Long> preNativeDelayedTask = new Pair<>(task, delay);
                mPreNativeDelayedTasks.add(preNativeDelayedTask);
            }
        }
    }

    protected Boolean belongsToCurrentThreadInternal() {
        // TODO(https://crbug.com/1026641): This function shouldn't be here, and should only be used
        // by derived classes (eg. SingleThreadTaskRunner) until it is moved there, as TaskRunner
        // has no notion of belonging to a thread.
        assert !getClass().equals(TaskRunnerImpl.class);
        synchronized (mPreNativeTaskLock) {
            oneTimeInitialization();
        }
        if (mNativeTaskRunnerAndroid == 0) return null;
        return TaskRunnerImplJni.get().belongsToCurrentThread(mNativeTaskRunnerAndroid);
    }

    @GuardedBy("mPreNativeTaskLock")
    private void oneTimeInitialization() {
        if (mDidOneTimeInitialization) return;
        mDidOneTimeInitialization = true;
        if (!PostTask.registerPreNativeTaskRunner(this)) {
            initNativeTaskRunner();
        } else {
            mPreNativeTasks = new LinkedList<>();
            mPreNativeDelayedTasks = new ArrayList<>();
        }
    }

    /**
     * Must be overridden in subclasses, schedules a call to runPreNativeTask() at an appropriate
     * time.
     */
    protected void schedulePreNativeTask() {
        PostTask.getPrenativeThreadPoolExecutor().execute(mRunPreNativeTaskClosure);
    }

    /**
     * Runs a single task and returns when its finished.
     */
    // The trace event name is derived from string literals.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    protected void runPreNativeTask() {
        try (TraceEvent te = TraceEvent.scoped(mTraceEvent)) {
            Runnable task;
            synchronized (mPreNativeTaskLock) {
                if (mPreNativeTasks == null) return;
                task = mPreNativeTasks.poll();
            }
            switch (mTaskTraits.mPriority) {
                case TaskPriority.USER_VISIBLE:
                    Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);
                    break;
                case TaskPriority.HIGHEST:
                    Process.setThreadPriority(Process.THREAD_PRIORITY_MORE_FAVORABLE);
                    break;
                default:
                    Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
                    break;
            }
            task.run();
        }
    }

    /**
     * Instructs the TaskRunner to initialize the native TaskRunner and migrate any tasks over to
     * it.
     */
    /* package */ void initNativeTaskRunner() {
        long nativeTaskRunnerAndroid = TaskRunnerImplJni.get().init(mTaskRunnerType,
                mTaskTraits.mPriority, mTaskTraits.mMayBlock, mTaskTraits.mUseThreadPool,
                mTaskTraits.mExtensionId, mTaskTraits.mExtensionData);
        synchronized (mPreNativeTaskLock) {
            if (mPreNativeTasks != null) {
                for (Runnable task : mPreNativeTasks) {
                    TaskRunnerImplJni.get().postDelayedTask(
                            nativeTaskRunnerAndroid, task, 0, task.getClass().getName());
                }
                mPreNativeTasks = null;
            }
            if (mPreNativeDelayedTasks != null) {
                for (Pair<Runnable, Long> task : mPreNativeDelayedTasks) {
                    TaskRunnerImplJni.get().postDelayedTask(nativeTaskRunnerAndroid, task.first,
                            task.second, task.getClass().getName());
                }
                mPreNativeDelayedTasks = null;
            }

            // mNativeTaskRunnerAndroid is volatile and setting this indicates we've have migrated
            // all pre-native tasks and are ready to use the native Task Runner.
            assert mNativeTaskRunnerAndroid == 0;
            mNativeTaskRunnerAndroid = nativeTaskRunnerAndroid;
        }
        synchronized (sCleaners) {
            sCleaners.add(new TaskRunnerCleaner(this));
        }

        // Destroying GC'd task runners here isn't strictly necessary, but the performance of
        // initNativeTaskRunner() isn't critical, and calling the function more often will help
        // prevent any potential build-up of orphaned native task runners.
        destroyGarbageCollectedTaskRunners();
    }


    @NativeMethods
    interface Natives {
        // NB due to Proguard obfuscation it's easiest to pass the traits via arguments.
        long init(@TaskRunnerType int taskRunnerType, int priority, boolean mayBlock,
                boolean useThreadPool, byte extensionId, byte[] extensionData);

        void destroy(long nativeTaskRunnerAndroid);
        void postDelayedTask(
                long nativeTaskRunnerAndroid, Runnable task, long delay, String runnableClassName);
        boolean belongsToCurrentThread(long nativeTaskRunnerAndroid);
    }
}
