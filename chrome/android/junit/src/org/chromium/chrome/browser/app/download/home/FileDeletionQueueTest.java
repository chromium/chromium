// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.concurrent.Semaphore;

/** Unit tests for the FileDeletionQueue class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FileDeletionQueueTest {
    @Mock
    public Callback<String> mDeleter;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private CallbackWrapper mWrappedDeleter;

    @Before
    public void setUp() {
        mWrappedDeleter = new CallbackWrapper(mDeleter);
    }

    @After
    public void tearDown() {
        mWrappedDeleter = null;
    }

    @Test
    public void testSingleDeletion() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete("test");

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult("test");
    }

    @Test
    public void testMultipleDeletion() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete("test1");
        queue.delete("test2");
        queue.delete("test3");

        mWrappedDeleter.waitFor(3);
        verify(mDeleter, times(1)).onResult("test1");
        verify(mDeleter, times(1)).onResult("test2");
        verify(mDeleter, times(1)).onResult("test3");
    }

    @Test
    public void testMultipleDeletionsAPI() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(Arrays.asList("test1", "test2", "test3"));

        mWrappedDeleter.waitFor(3);
        verify(mDeleter, times(1)).onResult("test1");
        verify(mDeleter, times(1)).onResult("test2");
        verify(mDeleter, times(1)).onResult("test3");
    }

    @Test
    public void testOneDeletionHappensAtATime() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(Arrays.asList("test1", "test2", "test3"));

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult("test1");

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult("test2");

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult("test3");
    }

    private static class CallbackWrapper implements Callback<String> {
        private final Callback<String> mWrappedCallback;
        private final Semaphore mDeletedSemaphore = new Semaphore(0);

        public CallbackWrapper(Callback<String> wrappedCallback) {
            mWrappedCallback = wrappedCallback;
        }

        public void waitFor(int calls) {
            long time = System.currentTimeMillis();

            while (!mDeletedSemaphore.tryAcquire(calls)) {
                if (time - System.currentTimeMillis() > 3000) Assert.fail();
                ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
                Robolectric.flushBackgroundThreadScheduler();
            }
        }

        // Callback<String> implementation.
        @Override
        public void onResult(String result) {
            ThreadUtils.assertOnBackgroundThread();
            mWrappedCallback.onResult(result);
            mDeletedSemaphore.release();
        }
    }
}
