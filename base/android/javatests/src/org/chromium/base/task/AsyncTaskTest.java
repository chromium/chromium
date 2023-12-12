// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.hamcrest.CoreMatchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

/**
 * Tests for our AsyncTask modifications
 *
 * Not a robolectric test because the reflection doesn't work with ShadowAsyncTask.
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

    // TODO(ksolt): do we need any post execution tests here?
}
