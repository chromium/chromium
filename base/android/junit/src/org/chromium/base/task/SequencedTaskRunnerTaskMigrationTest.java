// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Unit tests for {@link SequencedTaskRunnerImpl} that focuses on the transition/migration that
 * happens as native initializes.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SequencedTaskRunnerTaskMigrationTest {
    @Rule public JniMocker mMocker = new JniMocker();

    // It might be tempting to use fake executor similar to Robolectric's scheduler that is driven
    // from the test's main thread. Unfortunately this approach means that only two states of the
    // TaskRunner are observable: the posted task resides in the internal queue or the task is
    // removed from the queue and has its execution completed. The tricky case is the another state:
    // the task is already removed but is not yet completed. This can only be modelled with real
    // concurrency.
    private final ExecutorService mConcurrentExecutor = Executors.newCachedThreadPool();

    @Before
    public void setUp() throws Exception {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mConcurrentExecutor);
    }

    @After
    public void tearDown() throws Exception {
        // Ensure that no stuck threads left behind.
        List<Runnable> queuedRunnables = mConcurrentExecutor.shutdownNow();
        Assert.assertTrue("Some task is stuck in thread pool queue", queuedRunnables.isEmpty());
        // Termination will be immediate if tests aren't broken. Generous timeout prevents test
        // from being stuck forever.
        Assert.assertTrue(
                "Some task is stuck in thread pool",
                mConcurrentExecutor.awaitTermination(10, TimeUnit.SECONDS));
    }

    @Test
    public void nativeRunnerShouldNotExecuteTasksIfJavaThreadIsWorking() {
        Executor noopExecutor = runnable -> {};
        FakeTaskRunnerImplNatives fakeTaskRunnerNatives =
                new FakeTaskRunnerImplNatives(noopExecutor);
        mMocker.mock(TaskRunnerImplJni.TEST_HOOKS, fakeTaskRunnerNatives);
        BlockingTask preNativeTask = new BlockingTask();
        SequencedTaskRunnerImpl taskRunner = new SequencedTaskRunnerImpl(TaskTraits.USER_VISIBLE);

        taskRunner.execute(preNativeTask);
        // Empty task that is planned to be executed on native pool.
        taskRunner.execute(CallbackUtils.emptyRunnable());

        // Ensure that first task is running on pre-native thread pool: avoid race between
        // starting the task and requesting native task runner's init.
        preNativeTask.awaitTaskStarted();
        taskRunner.initNativeTaskRunner();

        Assert.assertFalse(
                "Native task should not start before java task completion",
                fakeTaskRunnerNatives.hasReceivedTasks());
    }

    @Test
    public void pendingTasksShouldBeExecutedOnNativeRunnerAfterInit() {
        FakeTaskRunnerImplNatives fakeTaskRunnerNatives =
                new FakeTaskRunnerImplNatives(mConcurrentExecutor);
        mMocker.mock(TaskRunnerImplJni.TEST_HOOKS, fakeTaskRunnerNatives);
        BlockingTask preNativeTask = new BlockingTask();
        AwaitableTask nativeTask = new AwaitableTask();
        SequencedTaskRunnerImpl taskRunner = new SequencedTaskRunnerImpl(TaskTraits.USER_VISIBLE);

        taskRunner.execute(preNativeTask);
        taskRunner.execute(nativeTask);

        // Ensure that first task is running on pre-native thread pool: avoid race between
        // starting the task and requesting native task runner's init.
        preNativeTask.awaitTaskStarted();
        taskRunner.initNativeTaskRunner();
        // Allow pre-native task to complete. Second task is going to be run on native pool because
        // native task runner is available.
        preNativeTask.allowComplete();

        // Wait for second task to be started: avoid race between submitting task to the native task
        // runner and checking the state of the latter in assertion below.
        nativeTask.awaitTaskStarted();

        Assert.assertTrue(
                "Second task should run on the native pool",
                fakeTaskRunnerNatives.hasReceivedTasks());
    }

    @Test
    public void taskPostedAfterNativeInitShouldRunInNativePool() {
        FakeTaskRunnerImplNatives fakeTaskRunnerNatives =
                new FakeTaskRunnerImplNatives(mConcurrentExecutor);
        mMocker.mock(TaskRunnerImplJni.TEST_HOOKS, fakeTaskRunnerNatives);

        SequencedTaskRunnerImpl taskRunner = new SequencedTaskRunnerImpl(TaskTraits.USER_VISIBLE);
        taskRunner.initNativeTaskRunner();

        AwaitableTask nativeTask = new AwaitableTask();
        taskRunner.execute(nativeTask);

        // Wait for the task to be started: avoid race between submitting task to the native task
        // runner and checking the state of the latter in assertion below.
        nativeTask.awaitTaskStarted();
        Assert.assertTrue(
                "Task should run on the native pool", fakeTaskRunnerNatives.hasReceivedTasks());
    }

    private static void awaitNoInterruptedException(CountDownLatch taskLatch) {
        try {
            // Generous timeout prevents test from being stuck forever. Actual delay is going to
            // be a few milliseconds.
            Assert.assertTrue(
                    "Timed out waiting for latch to count down",
                    taskLatch.await(10, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private static class AwaitableTask implements Runnable {
        private final CountDownLatch mTaskStartedLatch = new CountDownLatch(1);

        @Override
        public void run() {
            mTaskStartedLatch.countDown();
        }

        public void awaitTaskStarted() {
            awaitNoInterruptedException(mTaskStartedLatch);
        }
    }

    private static class BlockingTask extends AwaitableTask {
        private final CountDownLatch mTaskAllowedToComplete = new CountDownLatch(1);

        @Override
        public void run() {
            super.run();
            awaitNoInterruptedException(mTaskAllowedToComplete);
        }

        public void allowComplete() {
            mTaskAllowedToComplete.countDown();
        }
    }

    private static class FakeTaskRunnerImplNatives implements TaskRunnerImpl.Natives {
        private final AtomicInteger mReceivedTasksCount = new AtomicInteger();
        private final Executor mExecutor;

        public FakeTaskRunnerImplNatives(Executor executor) {
            mExecutor = executor;
        }

        @Override
        public long init(int taskRunnerType, int taskTraits) {
            return 1;
        }

        @Override
        public void destroy(long nativeTaskRunnerAndroid) {}

        @Override
        public void postDelayedTask(
                long nativeTaskRunnerAndroid, Runnable task, long delay, String runnableClassName) {
            mReceivedTasksCount.incrementAndGet();
            mExecutor.execute(task);
        }

        public boolean hasReceivedTasks() {
            return mReceivedTasksCount.get() > 0;
        }
    }
}
