// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask.Status;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.concurrent.CancellationException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Tests for {@link AsyncTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncTaskThreadTest {
    private static final String TAG = "AsyncTaskThreadTest";
    private static final boolean DEBUG = false;

    private static class BlockAndGetFeedDataTask extends AsyncTask<Boolean> {
        private final LinkedBlockingQueue<Boolean> mIncomingQueue =
                new LinkedBlockingQueue<Boolean>();
        private final LinkedBlockingQueue<Boolean> mOutgoingQueue =
                new LinkedBlockingQueue<Boolean>();
        private final LinkedBlockingQueue<Boolean> mInterruptedExceptionQueue =
                new LinkedBlockingQueue<Boolean>();
        private Boolean mPostExecuteResult;

        @Override
        protected Boolean doInBackground() {
            if (DEBUG) Log.i(TAG, "doInBackground");
            mOutgoingQueue.add(true);
            return blockAndGetFeedData();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (DEBUG) Log.i(TAG, "onPostExecute: " + result);
            mPostExecuteResult = result;
        }

        public void feedData(Boolean data) {
            mIncomingQueue.add(data);
        }

        private Boolean blockAndGetFeedData() {
            try {
                return mIncomingQueue.poll(3, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                if (DEBUG) Log.i(TAG, "InterruptedException");
                mInterruptedExceptionQueue.add(true);
                return false;
            }
        }

        public void blockUntilDoInBackgroundStarts() throws Exception {
            mOutgoingQueue.poll(3, TimeUnit.SECONDS);
        }

        public Boolean getPostExecuteResult() {
            return mPostExecuteResult;
        }

        public LinkedBlockingQueue<Boolean> getInterruptedExceptionQueue() {
            return mInterruptedExceptionQueue;
        }
    }

    private final BlockAndGetFeedDataTask mTask = new BlockAndGetFeedDataTask();

    @Rule public ExpectedException thrown = ExpectedException.none();

    public AsyncTaskThreadTest() {}

    @Before
    public void setUp() {
        RobolectricUtil.uninstallPausedExecutorService();
        assertEquals(Status.PENDING, mTask.getStatus());
    }

    @After
    public void tearDown() {
        // No unexpected interrupted exception.
        assertNull(mTask.getInterruptedExceptionQueue().poll());
    }

    @Test
    @SmallTest
    public void testCancel_ReturnsFalseOnceTaskFinishes() throws Exception {
        // This test requires robo executor service such that we can run
        // one background task.
        mTask.executeOnExecutor(RobolectricUtil.getPausedExecutor());

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        RobolectricUtil.runAllBackgroundAndUi();

        // Cannot cancel. The task is already run.
        assertFalse(mTask.cancel(/* mayInterruptIfRunning= */ false));
        assertTrue(mTask.get());
        assertEquals(true, mTask.getPostExecuteResult());

        // Note: This is somewhat counter-intuitive since cancel() failed.
        assertTrue(mTask.isCancelled());
        assertEquals(Status.FINISHED, mTask.getStatus());
    }

    @Test
    @SmallTest
    public void testCancel_InPreExecute() throws Exception {
        // Note that background loop is paused.
        mTask.executeOnExecutor(RobolectricUtil.getPausedExecutor());

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        // cancel() can still return true
        assertTrue(mTask.cancel(false));

        RobolectricUtil.runAllBackgroundAndUi();

        try {
            assertTrue(mTask.get());
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }

        assertTrue(mTask.isCancelled());
        assertEquals(Status.FINISHED, mTask.getStatus());
    }

    @Test
    @SmallTest
    public void testCancel_CanReturnTrueEvenAfterTaskStarts() throws Exception {
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Wait until the task is started. Note that data is not yet fed.
        mTask.blockUntilDoInBackgroundStarts();
        assertEquals(Status.RUNNING, mTask.getStatus());

        // This reflects FutureTask#cancel() behavior. Note that the task is
        // started but cancel can still return true.
        assertTrue(mTask.cancel(/* mayInterruptIfRunning= */ false));

        // Continue the task.
        mTask.feedData(true);

        // get() will raise an exception although the task is started.
        try {
            mTask.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }
        assertNull(mTask.getPostExecuteResult()); // onPostExecute did not run.

        assertTrue(mTask.isCancelled());
        assertEquals(Status.RUNNING, mTask.getStatus());
    }

    @Test
    @SmallTest
    public void testCancel_MayInterrupt_ReturnsFalseOnceTaskFinishes() throws Exception {
        // This test requires robo executor service such that we can run
        // one background task.
        mTask.executeOnExecutor(RobolectricUtil.getPausedExecutor());

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        RobolectricUtil.runAllBackgroundAndUi();

        // Cannot cancel. The task is already run.
        assertFalse(mTask.cancel(/* mayInterruptIfRunning= */ true));
        assertTrue(mTask.get());
        assertEquals(true, mTask.getPostExecuteResult());

        // Note: This is somewhat counter-intuitive since cancel() failed.
        assertTrue(mTask.isCancelled());

        assertEquals(Status.FINISHED, mTask.getStatus());
    }

    @Test
    @SmallTest
    public void testCancel_MayInterrupt_TaskIsInterrupted() throws Exception {
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Wait until the task is started. Note that data is not yet fed.
        mTask.blockUntilDoInBackgroundStarts();

        assertEquals(Status.RUNNING, mTask.getStatus());

        // Cancel and interrupt the current task.
        assertTrue(mTask.cancel(/* mayInterruptIfRunning= */ true));

        // Do not feed data here because task may finish before it gets interrupted.

        // get() will raise an exception although the task is started.
        try {
            mTask.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }
        assertNull(mTask.getPostExecuteResult()); // onPostExecute did not run.
        // Task was interrupted.
        // Note: interruption is raised and handled in the background thread, so we need to
        // wait here.
        assertEquals(true, mTask.getInterruptedExceptionQueue().poll(3, TimeUnit.SECONDS));

        assertTrue(mTask.isCancelled());
        assertEquals(Status.RUNNING, mTask.getStatus());
    }

    @Test
    @SmallTest
    public void testExecuteTwiceRaisesException() throws Exception {
        mTask.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        // Note that background loop is paused.
        try {
            // A second run should cause an exception.
            mTask.executeOnExecutor(RobolectricUtil.getPausedExecutor());
            Assert.fail();
        } catch (IllegalStateException e) {
            // expected
        }
        RobolectricUtil.runAllBackgroundAndUi(); // ensure to pass tearDown check
    }
}
