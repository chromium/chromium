// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Binder;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.WorkerThread;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.DoNotInline;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.RejectedExecutionHandler;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A Chromium version of android.os.AsyncTask.
 *
 * The API is quite close to Android's Oreo version, but with a number of things removed.
 * @param <Result> Return type of the background task.
 */
public abstract class AsyncTask<Result> {
    private static final String TAG = "AsyncTask";

    private static final String GET_STATUS_UMA_HISTOGRAM =
            "Android.Jank.AsyncTaskGetOnUiThreadStatus";

    /**
     * An {@link Executor} that can be used to execute tasks in parallel.
     * We use the lowest task priority, and mayBlock = true since any user of this could
     * block.
     */
    public static final Executor THREAD_POOL_EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, r);

    /**
     * An {@link Executor} that executes tasks one at a time in serial
     * order.  This serialization is global to a particular process.
     */
    public static final Executor SERIAL_EXECUTOR = new SerialExecutor();

    private static final StealRunnableHandler STEAL_RUNNABLE_HANDLER = new StealRunnableHandler();

    private final Callable<Result> mWorker;
    private final NamedFutureTask mFuture;

    private volatile @Status int mStatus = Status.PENDING;

    private final AtomicBoolean mCancelled = new AtomicBoolean();
    private final AtomicBoolean mTaskInvoked = new AtomicBoolean();
    private int mIterationIdForTesting = PostTask.sTestIterationForTesting;

    private static class StealRunnableHandler implements RejectedExecutionHandler {
        @Override
        public void rejectedExecution(Runnable r, ThreadPoolExecutor executor) {
            THREAD_POOL_EXECUTOR.execute(r);
        }
    }

    /**
     * Indicates the current status of the task. Each status will be set only once during the
     * lifetime of a task. AsyncTaskStatus corresponding to this is defined in
     * tools/metrics/histograms/enums.xml. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({Status.PENDING, Status.RUNNING, Status.FINISHED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Status {
        /** Indicates that the task has not been executed yet. */
        int PENDING = 0;

        /** Indicates that the task is running. */
        int RUNNING = 1;

        /** Indicates that {@link AsyncTask#onPostExecute} has finished. */
        int FINISHED = 2;

        /** Just used for reporting this status to UMA. */
        int NUM_ENTRIES = 3;
    }

    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    public static void takeOverAndroidThreadPool() {
        ThreadPoolExecutor exec = (ThreadPoolExecutor) android.os.AsyncTask.THREAD_POOL_EXECUTOR;
        exec.setRejectedExecutionHandler(STEAL_RUNNABLE_HANDLER);
        exec.shutdown();
    }

    /** Creates a new asynchronous task. This constructor must be invoked on the UI thread. */
    public AsyncTask() {
        mWorker =
                () -> {
                    mTaskInvoked.set(true);
                    Result result = null;
                    try {
                        result = doInBackground();
                        Binder.flushPendingCommands();
                    } catch (Throwable tr) {
                        mCancelled.set(true);
                        throw tr;
                    } finally {
                        postResult(result);
                    }
                    return result;
                };

        mFuture = new NamedFutureTask(mWorker);
    }

    private void postResultIfNotInvoked(Result result) {
        final boolean wasTaskInvoked = mTaskInvoked.get();
        if (!wasTaskInvoked) {
            postResult(result);
        }
    }

    private void postResult(Result result) {
        // We check if this task is of a type which does not require post-execution.
        if (this instanceof BackgroundOnlyAsyncTask) {
            mStatus = Status.FINISHED;
        } else if (mIterationIdForTesting == PostTask.sTestIterationForTesting) {
            ThreadUtils.postOnUiThread(
                    () -> {
                        finish(result);
                    });
        }
    }

    /**
     * Returns the current status of this task.
     *
     * @return The current status.
     */
    public final @Status int getStatus() {
        return mStatus;
    }

    /**
     * Returns the current status of this task, with adjustments made to make UMA more useful.
     * Namely, we are going to return "PENDING" until the asynctask actually starts running. Right
     * now, as soon as you try to schedule the AsyncTask, it gets set to "RUNNING" which doesn't
     * make sense. However, we aren't fixing this globally as this is the well-defined API
     * AsyncTasks have, so we are just fixing this for our UMA reporting.
     *
     * @return The current status.
     */
    public final @Status int getUmaStatus() {
        if (mStatus == Status.RUNNING && !mTaskInvoked.get()) return Status.PENDING;
        return mStatus;
    }

    /**
     * Override this method to perform a computation on a background thread.
     *
     * @return A result, defined by the subclass of this task.
     *
     * @see #onPreExecute()
     * @see #onPostExecute
     */
    @WorkerThread
    protected abstract Result doInBackground();

    /**
     * Runs on the UI thread before {@link #doInBackground}.
     *
     * @see #onPostExecute
     * @see #doInBackground
     */
    @MainThread
    protected void onPreExecute() {}

    /**
     * <p>Runs on the UI thread after {@link #doInBackground}. The
     * specified result is the value returned by {@link #doInBackground}.</p>
     *
     * <p>This method won't be invoked if the task was cancelled.</p>
     *
     * <p> Must be overridden by subclasses. If a subclass doesn't need
     * post-execution, is should extend BackgroundOnlyAsyncTask instead.
     *
     * @param result The result of the operation computed by {@link #doInBackground}.
     *
     * @see #onPreExecute
     * @see #doInBackground
     * @see #onCancelled(Object)
     */
    @SuppressWarnings({"UnusedDeclaration"})
    @MainThread
    protected abstract void onPostExecute(Result result);

    /**
     * <p>Runs on the UI thread after {@link #cancel(boolean)} is invoked and
     * {@link #doInBackground()} has finished.</p>
     *
     * <p>The default implementation simply invokes {@link #onCancelled()} and
     * ignores the result. If you write your own implementation, do not call
     * <code>super.onCancelled(result)</code>.</p>
     *
     * @param result The result, if any, computed in
     *               {@link #doInBackground()}, can be null
     *
     * @see #cancel(boolean)
     * @see #isCancelled()
     */
    @SuppressWarnings({"UnusedParameters"})
    @MainThread
    protected void onCancelled(Result result) {
        onCancelled();
    }

    /**
     * <p>Applications should preferably override {@link #onCancelled(Object)}.
     * This method is invoked by the default implementation of
     * {@link #onCancelled(Object)}.</p>
     *
     * <p>Runs on the UI thread after {@link #cancel(boolean)} is invoked and
     * {@link #doInBackground()} has finished.</p>
     *
     * @see #onCancelled(Object)
     * @see #cancel(boolean)
     * @see #isCancelled()
     */
    @MainThread
    protected void onCancelled() {}

    /**
     * Returns <tt>true</tt> if this task was cancelled before it completed
     * normally. If you are calling {@link #cancel(boolean)} on the task,
     * the value returned by this method should be checked periodically from
     * {@link #doInBackground()} to end the task as soon as possible.
     *
     * @return <tt>true</tt> if task was cancelled before it completed
     *
     * @see #cancel(boolean)
     */
    public final boolean isCancelled() {
        return mCancelled.get();
    }

    /**
     * <p>Attempts to cancel execution of this task.  This attempt will
     * fail if the task has already completed, already been cancelled,
     * or could not be cancelled for some other reason. If successful,
     * and this task has not started when <tt>cancel</tt> is called,
     * this task should never run. If the task has already started,
     * then the <tt>mayInterruptIfRunning</tt> parameter determines
     * whether the thread executing this task should be interrupted in
     * an attempt to stop the task.</p>
     *
     * <p>Calling this method will result in {@link #onCancelled(Object)} being
     * invoked on the UI thread after {@link #doInBackground()}
     * returns. Calling this method guarantees that {@link #onPostExecute(Object)}
     * is never invoked. After invoking this method, you should check the
     * value returned by {@link #isCancelled()} periodically from
     * {@link #doInBackground()} to finish the task as early as
     * possible.</p>
     *
     * @param mayInterruptIfRunning <tt>true</tt> if the thread executing this
     *        task should be interrupted; otherwise, in-progress tasks are allowed
     *        to complete.
     *
     * @return <tt>false</tt> if the task could not be cancelled,
     *         typically because it has already completed normally;
     *         <tt>true</tt> otherwise
     *
     * @see #isCancelled()
     * @see #onCancelled(Object)
     */
    public final boolean cancel(boolean mayInterruptIfRunning) {
        mCancelled.set(true);
        return mFuture.cancel(mayInterruptIfRunning);
    }

    /**
     * Waits if necessary for the computation to complete, and then
     * retrieves its result.
     *
     * @return The computed result.
     *
     * @throws CancellationException If the computation was cancelled.
     * @throws ExecutionException If the computation threw an exception.
     * @throws InterruptedException If the current thread was interrupted
     *         while waiting.
     */
    @DoNotInline
    // The string passed is safe since it is class and method name.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    public final Result get() throws InterruptedException, ExecutionException {
        Result r;
        int status = getUmaStatus();
        if (status != Status.FINISHED && ThreadUtils.runningOnUiThread()) {
            RecordHistogram.recordEnumeratedHistogram(
                    GET_STATUS_UMA_HISTOGRAM, status, Status.NUM_ENTRIES);
            StackTraceElement[] stackTrace = new Exception().getStackTrace();
            String caller = "";
            if (stackTrace.length > 1) {
                caller = stackTrace[1].getClassName() + '.' + stackTrace[1].getMethodName() + '.';
            }
            try (TraceEvent e = TraceEvent.scoped(caller + "AsyncTask.get")) {
                r = mFuture.get();
            }
        } else {
            r = mFuture.get();
        }
        return r;
    }

    /**
     * Waits if necessary for at most the given time for the computation to complete, and then
     * retrieves its result.
     *
     * @param timeout Time to wait before cancelling the operation.
     * @param unit The time unit for the timeout.
     *
     * @return The computed result.
     *
     * @throws CancellationException If the computation was cancelled.
     * @throws ExecutionException If the computation threw an exception.
     * @throws InterruptedException If the current thread was interrupted while waiting.
     * @throws TimeoutException If the wait timed out.
     */
    @DoNotInline
    // The string passed is safe since it is class and method name.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    public final Result get(long timeout, TimeUnit unit)
            throws InterruptedException, ExecutionException, TimeoutException {
        Result r;
        int status = getUmaStatus();
        if (status != Status.FINISHED && ThreadUtils.runningOnUiThread()) {
            RecordHistogram.recordEnumeratedHistogram(
                    GET_STATUS_UMA_HISTOGRAM, status, Status.NUM_ENTRIES);
            StackTraceElement[] stackTrace = new Exception().getStackTrace();
            String caller = "";
            if (stackTrace.length > 1) {
                caller = stackTrace[1].getClassName() + '.' + stackTrace[1].getMethodName() + '.';
            }
            try (TraceEvent e = TraceEvent.scoped(caller + "AsyncTask.get")) {
                r = mFuture.get(timeout, unit);
            }
        } else {
            r = mFuture.get(timeout, unit);
        }
        return r;
    }

    @SuppressWarnings({"MissingCasesInEnumSwitch"})
    private void executionPreamble() {
        if (mStatus != Status.PENDING) {
            switch (mStatus) {
                case Status.RUNNING:
                    throw new IllegalStateException(
                            "Cannot execute task:" + " the task is already running.");
                case Status.FINISHED:
                    throw new IllegalStateException(
                            "Cannot execute task:"
                                    + " the task has already been executed "
                                    + "(a task can be executed only once)");
            }
        }

        mStatus = Status.RUNNING;

        onPreExecute();
    }

    /**
     * Executes the task with the specified parameters. The task returns
     * itself (this) so that the caller can keep a reference to it.
     *
     * <p>This method is typically used with {@link #THREAD_POOL_EXECUTOR} to
     * allow multiple tasks to run in parallel on a pool of threads managed by
     * AsyncTask, however you can also use your own {@link Executor} for custom
     * behavior.
     *
     * <p><em>Warning:</em> Allowing multiple tasks to run in parallel from
     * a thread pool is generally <em>not</em> what one wants, because the order
     * of their operation is not defined.  For example, if these tasks are used
     * to modify any state in common (such as writing a file due to a button click),
     * there are no guarantees on the order of the modifications.
     * Without careful work it is possible in rare cases for the newer version
     * of the data to be over-written by an older one, leading to obscure data
     * loss and stability issues.  Such changes are best
     * executed in serial; to guarantee such work is serialized regardless of
     * platform version you can use this function with {@link #SERIAL_EXECUTOR}.
     *
     * <p>This method must be invoked on the UI thread.
     *
     * @param exec The executor to use.  {@link #THREAD_POOL_EXECUTOR} is available as a
     *              convenient process-wide thread pool for tasks that are loosely coupled.
     *
     * @return This instance of AsyncTask.
     *
     * @throws IllegalStateException If {@link #getStatus()} returns either
     *         {@link AsyncTask.Status#RUNNING} or {@link AsyncTask.Status#FINISHED}.
     */
    @MainThread
    public final AsyncTask<Result> executeOnExecutor(Executor exec) {
        executionPreamble();
        exec.execute(mFuture);
        return this;
    }

    /**
     * Executes an AsyncTask on the given TaskRunner.
     *
     * @param taskRunner taskRunner to run this AsyncTask on.
     * @return This instance of AsyncTask.
     */
    @MainThread
    public final AsyncTask<Result> executeOnTaskRunner(TaskRunner taskRunner) {
        executionPreamble();
        taskRunner.execute(mFuture);
        return this;
    }

    /**
     * Executes an AsyncTask with the given task traits. Provides no guarantees about sequencing or
     * which thread it runs on.
     *
     * @param taskTraits traits which describe this AsyncTask.
     * @return This instance of AsyncTask.
     */
    @MainThread
    public final AsyncTask<Result> executeWithTaskTraits(@TaskTraits int taskTraits) {
        executionPreamble();
        PostTask.postTask(taskTraits, mFuture);
        return this;
    }

    private void finish(Result result) {
        if (isCancelled()) {
            onCancelled(result);
        } else {
            onPostExecute(result);
        }
        mStatus = Status.FINISHED;
    }

    class NamedFutureTask extends FutureTask<Result> {
        NamedFutureTask(Callable<Result> c) {
            super(c);
        }

        Class getBlamedClass() {
            return AsyncTask.this.getClass();
        }

        @Override
        @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
        public void run() {
            try (TraceEvent e =
                    TraceEvent.scoped("AsyncTask.run: " + mFuture.getBlamedClass().getName())) {
                super.run();
            } finally {
                // Clear the interrupt on this background thread, if there is one, as it likely
                // came from cancelling the FutureTask. It is possible this was already cleared
                // in run() if something was listening for an interrupt; however, if it wasn't
                // then the interrupt may still be around. By clearing it here the thread is in
                // a clean state for the next task. See: crbug/1473731.

                // This is safe and prevents future leaks because the state of the FutureTask
                // should now be >= COMPLETING. Any future calls to cancel() will not trigger
                // an interrupt.
                Thread.interrupted();
            }
        }

        @Override
        protected void done() {
            try {
                postResultIfNotInvoked(get());
            } catch (InterruptedException e) {
                Log.w(TAG, e.toString());
            } catch (ExecutionException e) {
                throw new RuntimeException(
                        "An error occurred while executing doInBackground()", e.getCause());
            } catch (CancellationException e) {
                postResultIfNotInvoked(null);
            }
        }
    }
}
