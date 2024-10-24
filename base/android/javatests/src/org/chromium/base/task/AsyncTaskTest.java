// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Process;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.hamcrest.CoreMatchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for our AsyncTask modifications
 *
 * <p>Not a robolectric test because the reflection doesn't work with ShadowAsyncTask.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AsyncTaskTest {
    private static class SpecialChromeAsyncTask extends BackgroundOnlyAsyncTask<Void> {
        @Override
        protected Void doInBackground() {
            return null;
        }
    }

    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private static class SpecialOsAsyncTask extends android.os.AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... params) {
            return null;
        }
    }

    private static class SpecialRunnable implements Runnable {
        @Override
        public void run() {}
    }

    private static final int QUEUE_SIZE = 40;

    @Rule public ExpectedException thrown = ExpectedException.none();

    /**
     * Test filling the queue with basic Runnables, then add a final AsyncTask to overfill it, and
     * ensure the Runnable is the one blamed in the exception message.
     */
    @Test
    @SmallTest
    public void testChromeThreadPoolExecutorRunnables() {
        Executor executor =
                new ChromeThreadPoolExecutor(
                        1,
                        1,
                        1,
                        TimeUnit.SECONDS,
                        new ArrayBlockingQueue<Runnable>(QUEUE_SIZE),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(@NonNull Runnable r) {
                                return null;
                            }
                        });
        for (int i = 0; i < QUEUE_SIZE; i++) {
            executor.execute(new SpecialRunnable());
        }
        thrown.expect(RejectedExecutionException.class);
        thrown.expectMessage(
                CoreMatchers.containsString(
                        "org.chromium.base.task.AsyncTaskTest$SpecialRunnable"));
        thrown.expectMessage(
                CoreMatchers.not(CoreMatchers.containsString("SpecialChromeAsyncTask")));
        new SpecialChromeAsyncTask().executeOnExecutor(executor);
    }

    /**
     * Test filling the queue with Chrome AsyncTasks, then add a final OS AsyncTask to
     * overfill it and ensure the Chrome AsyncTask is the one blamed in the exception message.
     */
    @Test
    @SmallTest
    public void testChromeThreadPoolExecutorChromeAsyncTask() {
        Executor executor =
                new ChromeThreadPoolExecutor(
                        1,
                        1,
                        1,
                        TimeUnit.SECONDS,
                        new ArrayBlockingQueue<Runnable>(QUEUE_SIZE),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(@NonNull Runnable r) {
                                return null;
                            }
                        });
        for (int i = 0; i < QUEUE_SIZE; i++) {
            new SpecialChromeAsyncTask().executeOnExecutor(executor);
        }
        thrown.expect(RejectedExecutionException.class);
        thrown.expectMessage(
                CoreMatchers.containsString(
                        "org.chromium.base.task.AsyncTaskTest$SpecialChromeAsyncTask"));
        thrown.expectMessage(CoreMatchers.not(CoreMatchers.containsString("android.os.AsyncTask")));
        new SpecialOsAsyncTask().executeOnExecutor(executor);
    }

    /**
     * Test filling the queue with android.os.AsyncTasks, then add a final ChromeAsyncTask to
     * overfill it and ensure the OsAsyncTask is the one blamed in the exception message.
     */
    @Test
    @SmallTest
    public void testChromeThreadPoolExecutorOsAsyncTask() {
        Executor executor =
                new ChromeThreadPoolExecutor(
                        1,
                        1,
                        1,
                        TimeUnit.SECONDS,
                        new ArrayBlockingQueue<Runnable>(QUEUE_SIZE),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(@NonNull Runnable r) {
                                return null;
                            }
                        });
        for (int i = 0; i < QUEUE_SIZE; i++) {
            new SpecialOsAsyncTask().executeOnExecutor(executor);
        }
        thrown.expect(RejectedExecutionException.class);
        thrown.expectMessage(CoreMatchers.containsString("android.os.AsyncTask"));
        thrown.expectMessage(
                CoreMatchers.not(CoreMatchers.containsString("SpecialChromeAsyncTask")));
        new SpecialChromeAsyncTask().executeOnExecutor(executor);
    }

    /**
     * Test verifying that tasks which specify that they are not using onPostExecute
     * don't trigger it.
     */
    @Test
    @SmallTest
    public void testTaskNotNeedingPostExecutionDoesNotTriggerIt() {
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                return null;
            }
            // Calling onPostExecute on this class causes failure.
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Checks that Android's AsyncTasks do not permanently reset their thread pool thread priority.
     */
    @Test
    @SmallTest
    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    public void testAsyncTaskThreadPriority() throws Exception {
        AsyncTask.takeOverAndroidThreadPool();
        var waitSemaphore = new Semaphore(0);
        final var threadPoolTid = new AtomicInteger(0);
        int invalidThreadPriority = Process.THREAD_PRIORITY_LOWEST - 1;
        final var backgroundThreadPriority = new AtomicInteger(invalidThreadPriority);
        final var threadPriorityAfterTask = new AtomicInteger(invalidThreadPriority);

        new android.os.AsyncTask<Void, Void, Void>() {
            @Override
            public Void doInBackground(Void... params) {
                backgroundThreadPriority.set(Process.getThreadPriority(Process.myTid()));
                threadPoolTid.set(Process.myTid());
                return null;
            }

            @Override
            public void onPostExecute(Void result) {
                // Releasing the semaphore here, to make sure that AsyncTask's code that executes in
                // the background thread after our code is finished.
                waitSemaphore.release();
            }
        }.execute();
        waitSemaphore.acquire();

        // Need to get the thread priority from a task running on the same thread, to check that the
        // priority is set correctly. Cannot be done from another thread, as the thread pool may be
        // currently running another AsyncTask.
        while (threadPriorityAfterTask.get() == invalidThreadPriority) {
            // Using the same executor, but not an AsyncTask, so that it doesn't interfere with
            // priorities.
            AsyncTask.THREAD_POOL_EXECUTOR.execute(
                    () -> {
                        if (threadPoolTid.get() == Process.myTid()) {
                            threadPriorityAfterTask.set(Process.getThreadPriority(Process.myTid()));
                        }
                        waitSemaphore.release();
                    });
            waitSemaphore.acquire();
        }

        // The thread priority is lowered while inside the task.
        Assert.assertEquals(Process.THREAD_PRIORITY_BACKGROUND, backgroundThreadPriority.get());
        // But restored afterwards.
        Assert.assertEquals(Process.THREAD_PRIORITY_DEFAULT, threadPriorityAfterTask.get());
    }

    // TODO(ksolt): do we need any post execution tests here?
}
