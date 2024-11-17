// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.JavaUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link ChainedTasks}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ChainedTasksTest {
    private static final class TestRunnable implements Runnable {
        private final List<String> mLogMessages;
        private final String mMessage;

        public TestRunnable(List<String> logMessages, String message) {
            mLogMessages = logMessages;
            mMessage = message;
        }

        @Override
        public void run() {
            mLogMessages.add(mMessage);
        }
    }

    @Test
    @SmallTest
    public void testCoalescedTasks() {
        final List<String> expectedMessages = List.of("First", "Second", "Third");
        final List<String> messages = new ArrayList<>();
        final ChainedTasks tasks = new ChainedTasks();
        for (String message : expectedMessages) {
            tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, message));
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tasks.start(true);
                    Assert.assertEquals(expectedMessages, messages);
                });
    }

    @Test
    @SmallTest
    public void testCoalescedTasksDontBlockNonUiThread() throws Exception {
        CallbackHelper waitForIt = new CallbackHelper();
        CallbackHelper finished = new CallbackHelper();
        ChainedTasks tasks = new ChainedTasks();

        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try {
                        waitForIt.waitForOnly();
                    } catch (TimeoutException e) {
                        JavaUtils.throwUnchecked(e);
                    }
                });

        List<String> expectedMessages = List.of("First", "Second", "Third");
        final List<String> messages = new ArrayList<>();
        for (String message : expectedMessages) {
            tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, message));
        }
        tasks.add(TaskTraits.UI_DEFAULT, finished::notifyCalled);

        tasks.start(true);
        // If start() were blocking, then this would be a deadlock, as the first task acquires a
        // semaphore that we are releasing later on the same thread.
        waitForIt.notifyCalled();
        finished.waitForOnly();
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testAsyncTasks() throws Exception {
        List<String> expectedMessages = List.of("First", "Second", "Third");
        List<String> messages = new ArrayList<>();
        ChainedTasks tasks = new ChainedTasks();
        CallbackHelper callbackHelper = new CallbackHelper();

        for (String message : expectedMessages) {
            tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, message));
        }
        tasks.add(TaskTraits.UI_DEFAULT, callbackHelper::notifyCalled);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tasks.start(false);
                    Assert.assertTrue("No task should run synchronously", messages.isEmpty());
                });
        callbackHelper.waitForOnly();
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testAsyncTasksAreChained() throws Exception {
        List<String> expectedMessages = List.of("First", "Second", "High Priority", "Third");
        List<String> messages = new ArrayList<>();
        ChainedTasks tasks = new ChainedTasks();
        CallbackHelper secondTaskFinished = new CallbackHelper();
        CallbackHelper waitForHighPriorityTask = new CallbackHelper();
        CallbackHelper finished = new CallbackHelper();

        // Posts 2 tasks, waits for a high priority task to be posted from another thread, and
        // carries on.
        tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, "First"));
        tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, "Second"));
        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try {
                        secondTaskFinished.notifyCalled();
                        waitForHighPriorityTask.waitForOnly();
                    } catch (TimeoutException e) {
                        throw new RuntimeException(e);
                    }
                });
        tasks.add(TaskTraits.UI_DEFAULT, new TestRunnable(messages, "Third"));
        tasks.add(TaskTraits.UI_DEFAULT, finished::notifyCalled);

        tasks.start(false);
        secondTaskFinished.waitForOnly();
        PostTask.postTask(TaskTraits.UI_DEFAULT, new TestRunnable(messages, "High Priority"));
        waitForHighPriorityTask.notifyCalled();
        finished.waitForOnly();
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testCanCancel() throws Exception {
        ChainedTasks tasks = new ChainedTasks();

        tasks.add(TaskTraits.UI_DEFAULT, tasks::cancel);
        tasks.add(TaskTraits.UI_DEFAULT, Assert::fail);
        tasks.start(false);
    }

    @Test
    @SmallTest
    public void testUncaughtException() throws Exception {
        ChainedTasks tasks = new ChainedTasks();
        AtomicReference<Throwable> uncaught = new AtomicReference<>();
        CallbackHelper callbackHelper = new CallbackHelper();
        tasks.add(TaskTraits.USER_BLOCKING, () -> {});
        tasks.add(
                TaskTraits.USER_BLOCKING,
                () -> {
                    Thread.currentThread()
                            .setUncaughtExceptionHandler(
                                    (thread, throwable) -> {
                                        uncaught.set(throwable);
                                        callbackHelper.notifyCalled();
                                    });
                    throw new Error("Error");
                });

        tasks.start(false);
        callbackHelper.waitForOnly();

        Throwable ex = uncaught.get();
        Assert.assertEquals("Error", ex.getMessage());

        Throwable actualTaskOrigin = ex.getCause();
        String assertMsg = "Was: " + Log.getStackTraceString(ex);
        if (PostTask.ENABLE_TASK_ORIGINS) {
            Assert.assertTrue(assertMsg, actualTaskOrigin instanceof TaskOriginException);
            // Ensure the origin is set to the ChainedTasks.add() call, and not the deferred
            // PostTask.
            Assert.assertTrue(Log.getStackTraceString(ex.getCause()).contains("ChainedTasksTest"));
        } else {
            Assert.assertNull(assertMsg, actualTaskOrigin);
        }
    }
}
