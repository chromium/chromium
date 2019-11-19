// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link ChainedTasks}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ChainedTasksTest {
    private static final long TIMEOUT_MS = 1000;

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
        final List<String> expectedMessages =
                Arrays.asList(new String[] {"First", "Second", "Third"});
        final List<String> messages = new ArrayList<>();
        final ChainedTasks tasks = new ChainedTasks();
        for (String message : expectedMessages) {
            tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, message));
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tasks.start(true);
            Assert.assertEquals(expectedMessages, messages);
        });
    }

    @Test
    @SmallTest
    public void testCoalescedTasksDontBlockNonUiThread() throws Exception {
        final Semaphore waitForIt = new Semaphore(0);
        final Semaphore finished = new Semaphore(0);
        final ChainedTasks tasks = new ChainedTasks();

        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                try {
                    waitForIt.acquire();
                } catch (InterruptedException e) {
                    throw new RuntimeException();
                }
            }
        });

        List<String> expectedMessages = Arrays.asList(new String[] {"First", "Second", "Third"});
        final List<String> messages = new ArrayList<>();
        for (String message : expectedMessages) {
            tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, message));
        }
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                finished.release();
            }
        });

        tasks.start(true);
        // If start() were blocking, then this would be a deadlock, as the first task acquires a
        // semaphore that we are releasing later on the same thread.
        waitForIt.release();
        Assert.assertTrue(finished.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testAsyncTasks() throws Exception {
        List<String> expectedMessages = Arrays.asList(new String[] {"First", "Second", "Third"});
        final List<String> messages = new ArrayList<>();
        final ChainedTasks tasks = new ChainedTasks();
        final Semaphore finished = new Semaphore(0);

        for (String message : expectedMessages) {
            tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, message));
        }
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                finished.release();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tasks.start(false);
            Assert.assertTrue("No task should run synchronously", messages.isEmpty());
        });
        Assert.assertTrue(finished.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testAsyncTasksAreChained() throws Exception {
        List<String> expectedMessages =
                Arrays.asList(new String[] {"First", "Second", "High Priority", "Third"});
        final List<String> messages = new ArrayList<>();
        final ChainedTasks tasks = new ChainedTasks();
        final Semaphore secondTaskFinished = new Semaphore(0);
        final Semaphore waitForHighPriorityTask = new Semaphore(0);
        final Semaphore finished = new Semaphore(0);

        // Posts 2 tasks, waits for a high priority task to be posted from another thread, and
        // carries on.
        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "First"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "Second"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                try {
                    secondTaskFinished.release();
                    waitForHighPriorityTask.acquire();
                } catch (InterruptedException e) {
                    Assert.fail();
                }
            }
        });
        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "Third"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                finished.release();
            }
        });

        tasks.start(false);
        Assert.assertTrue(secondTaskFinished.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "High Priority"));
        waitForHighPriorityTask.release();

        Assert.assertTrue(finished.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testCanCancel() throws Exception {
        List<String> expectedMessages = Arrays.asList(new String[] {"First", "Second"});
        final List<String> messages = new ArrayList<>();
        final ChainedTasks tasks = new ChainedTasks();
        final Semaphore finished = new Semaphore(0);

        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "First"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "Second"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                tasks.cancel();
            }
        });
        tasks.add(UiThreadTaskTraits.DEFAULT, new TestRunnable(messages, "Third"));
        tasks.add(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                finished.release();
            }
        });
        tasks.start(false);
        Assert.assertFalse(finished.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(expectedMessages, messages);
    }

    @Test
    @SmallTest
    public void testThreadRestrictions() {
        ChainedTasks tasks = new ChainedTasks();
        tasks.start(false);
        try {
            tasks.cancel();
            Assert.fail("Cancel should not be callable from a non-UI thread");
        } catch (IllegalStateException e) {
            // Expected.
        }
    }
}
