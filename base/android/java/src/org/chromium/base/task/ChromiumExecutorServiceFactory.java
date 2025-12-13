// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static java.util.concurrent.TimeUnit.MILLISECONDS;
import static java.util.concurrent.TimeUnit.NANOSECONDS;

import com.google.common.util.concurrent.ListeningScheduledExecutorService;
import com.google.common.util.concurrent.MoreExecutors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collection;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Delayed;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.RunnableScheduledFuture;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * Chromium's implementation of ExecutorService via the PostTask API.
 */
@NullMarked
public final class ChromiumExecutorServiceFactory {
    private ChromiumExecutorServiceFactory() {}

    /** Creates a {@link ScheduledExecutorService}. */
    public static ListeningScheduledExecutorService create(@TaskTraits int taskTraits) {
        TaskRunner taskRunner = PostTask.getTaskRunner(taskTraits);
        return create(taskRunner);
    }

    public static ListeningScheduledExecutorService create(TaskRunner taskRunner) {
        return MoreExecutors.listeningDecorator(new ScheduledExecutorServiceImpl(taskRunner));
    }

    private static long getCurrentTimeNanos() {
        return System.nanoTime();
    }

    @NullUnmarked // https://github.com/uber/NullAway/issues/1212
    private static final class ScheduledFutureTask<V extends @Nullable Object> extends FutureTask<V>
            implements RunnableScheduledFuture<V> {
        /** The time in nanoseconds when the task is scheduled to execute. */
        private long mNanoTaskScheduledStartTime;

        /** The time period in nanoseconds between successive executions of periodic tasks. */
        private final long mNanoTaskPeriod;

        /**
         * The delay in nanoseconds between the termination of one execution and
         * the commencement of the next periodic task.
         */
        private final long mNanoInterTaskDelay;

        /** The time in nanoseconds when the task is actually executed. */
        private long mNanoTaskActualStartTime;

        ScheduledFutureTask(Runnable runnable, V result, long nanoDelay) {
            super(runnable, result);
            mNanoTaskScheduledStartTime = getCurrentTimeNanos() + nanoDelay;
            mNanoTaskPeriod = 0;
            mNanoInterTaskDelay = 0;
        }

        ScheduledFutureTask(Callable<V> callable, long nanoDelay) {
            super(callable);
            mNanoTaskScheduledStartTime = getCurrentTimeNanos() + nanoDelay;
            mNanoTaskPeriod = 0;
            mNanoInterTaskDelay = 0;
        }

        ScheduledFutureTask(
                Runnable runnable,
                V result,
                long nanoInitialDelay,
                long nanoPeriod,
                long nanoInterTaskDelay) {
            super(runnable, result);
            // Make sure only one of nanoPeriod and nanoInterTaskDelay is set.
            assert (nanoPeriod != 0) ^ (nanoInterTaskDelay != 0);
            mNanoTaskScheduledStartTime = getCurrentTimeNanos() + nanoInitialDelay;
            mNanoTaskPeriod = nanoPeriod;
            mNanoInterTaskDelay = nanoInterTaskDelay;
        }

        /** Returns true if this is a periodic (not a one-shot) action. */
        @Override
        public boolean isPeriodic() {
            return mNanoTaskPeriod != 0 || mNanoInterTaskDelay != 0;
        }

        /** Returns the comparison result between this and other instance. */
        @Override
        public int compareTo(Delayed other) {
            if (other == this) {
                return 0;
            }
            return Long.compare(getDelay(NANOSECONDS), other.getDelay(NANOSECONDS));
        }

        /** Returns the remaining delay associated with this object, in the given time unit. */
        @Override
        public long getDelay(TimeUnit unit) {
            return unit.convert(mNanoTaskScheduledStartTime - getCurrentTimeNanos(), NANOSECONDS);
        }

        @Override
        public void run() {
            if (isCancelled()) {
                return;
            }
            mNanoTaskActualStartTime = getCurrentTimeNanos();
            if (!isPeriodic()) {
                super.run();
            } else if (super.runAndReset()) {
                setNextRunTime();
            }
        }

        private void setNextRunTime() {
            if (mNanoTaskPeriod != 0) {
                mNanoTaskScheduledStartTime = mNanoTaskActualStartTime + mNanoTaskPeriod;
            } else {
                mNanoTaskScheduledStartTime = getCurrentTimeNanos() + mNanoInterTaskDelay;
            }
        }
    }

    private static final class ScheduledExecutorServiceImpl implements ScheduledExecutorService {
        private final TaskRunner mTaskRunner;

        public ScheduledExecutorServiceImpl(TaskRunner taskRunner) {
            mTaskRunner = taskRunner;
        }

        @Override
        public ScheduledFuture<?> schedule(Runnable command, long delay, TimeUnit unit) {
            ScheduledFutureTask<@Nullable Void> t =
                    new ScheduledFutureTask<>(command, null, unit.toNanos(delay));
            postTask(t);
            return t;
        }

        @Override
        public <V extends @Nullable Object> ScheduledFuture<V> schedule(
                Callable<V> callable, long delay, TimeUnit unit) {
            ScheduledFutureTask<V> t = new ScheduledFutureTask<V>(callable, unit.toNanos(delay));
            postTask(t);
            return t;
        }

        @Override
        public ScheduledFuture<?> scheduleAtFixedRate(
                Runnable command, long initialDelay, long period, TimeUnit unit) {
            ScheduledFutureTask<@Nullable Void> t =
                    new ScheduledFutureTask<>(
                            command,
                            null,
                            unit.toNanos(initialDelay),
                            unit.toNanos(period),
                            /* nanoInterTaskDelay= */ 0);
            postTask(t);
            return t;
        }

        @Override
        public ScheduledFuture<?> scheduleWithFixedDelay(
                Runnable command, long initialDelay, long delay, TimeUnit unit) {
            ScheduledFutureTask<@Nullable Void> t =
                    new ScheduledFutureTask<>(
                            command,
                            null,
                            unit.toNanos(initialDelay),
                            /* nanoPeriod= */ 0,
                            unit.toNanos(delay));
            postTask(t);
            return t;
        }

        @Override
        public void shutdown() {
            throw new UnsupportedOperationException();
        }

        @Override
        public List<Runnable> shutdownNow() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isShutdown() {
            return false;
        }

        @Override
        public boolean isTerminated() {
            return false;
        }

        @Override
        public boolean awaitTermination(long timeout, TimeUnit unit) {
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> Future<T> submit(Callable<T> task) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> Future<T> submit(Runnable task, T result) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public Future<?> submit(Runnable task) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> List<Future<T>> invokeAll(Collection<? extends Callable<T>> tasks) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> List<Future<T>> invokeAll(
                Collection<? extends Callable<T>> tasks, long timeout, TimeUnit unit) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> T invokeAny(Collection<? extends Callable<T>> tasks) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public <T> T invokeAny(
                Collection<? extends Callable<T>> tasks, long timeout, TimeUnit unit) {
            // No need to support this since it is never called.
            // MoreExecutors.listeningDecorator routes the logic to execute().
            throw new UnsupportedOperationException();
        }

        @Override
        public void execute(Runnable runnable) {
            mTaskRunner.postDelayedTask(runnable, 0);
        }

        @NullUnmarked // https://github.com/uber/NullAway/issues/1075
        private <T extends @Nullable Object> void postTask(ScheduledFutureTask<T> task) {
            mTaskRunner.postDelayedTask(() -> runTask(task), task.getDelay(MILLISECONDS));
        }

        private <T extends @Nullable Object> void runTask(ScheduledFutureTask<T> task) {
            task.run();
            if (!task.isPeriodic() || task.isCancelled()) {
                return;
            }
            postTask(task);
        }
    }
}
