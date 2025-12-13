// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.os.Looper;

import com.google.common.util.concurrent.ListenableScheduledFuture;
import com.google.common.util.concurrent.ListeningScheduledExecutorService;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.time.Duration;
import java.util.concurrent.Callable;
import java.util.concurrent.Future;

@RunWith(BaseRobolectricTestRunner.class)
public final class ChromiumExecutorServiceFactoryTest {

    @SuppressWarnings("unchecked")
    private final Callable<Object> mMockCallable = mock(Callable.class);

    private final Runnable mMockRunnable = mock(Runnable.class);

    private ListeningScheduledExecutorService mExecutor;
    private final Runnable mSlowRunnable =
            spy(
                    new Runnable() {
                        @Override
                        public void run() {
                            shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(8));
                        }
                    });

    @Before
    public void setUp() {
        mExecutor = ChromiumExecutorServiceFactory.create(TaskTraits.UI_DEFAULT);
    }

    private void advanceTimeBy(long milliseconds) {
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(milliseconds));
    }

    @Test
    public void execute() {
        mExecutor.execute(mMockRunnable);
        verifyNoInteractions(mMockRunnable);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mMockRunnable).run();
    }

    @Test
    public void submit_executesRunnable() {
        var unused = mExecutor.submit(mMockRunnable);
        verifyNoInteractions(mMockRunnable);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mMockRunnable).run();
    }

    @Test
    public void submit_executesCallable() throws Exception {
        Object returnValue = new Object();
        when(mMockCallable.call()).thenReturn(returnValue);
        Future<Object> future = mExecutor.submit(mMockCallable);
        verifyNoInteractions(mMockCallable);
        assertFalse(future.isDone());
        shadowOf(Looper.getMainLooper()).idle();
        assertEquals(returnValue, future.get());
    }

    @Test
    public void schedule_executesRunnableImmediately() {
        @SuppressWarnings({"unused", "nullness"}) // go/futurereturn-lsc
        ListenableScheduledFuture<?> future = mExecutor.schedule(mMockRunnable, 0, SECONDS);
        verifyNoInteractions(mMockRunnable);
        assertFalse(future.isDone());

        shadowOf(Looper.getMainLooper()).idle();
        assertTrue(future.isDone());
        verify(mMockRunnable).run();
    }

    @Test
    public void schedule_executesRunnableWithDelay() {
        @SuppressWarnings({"unused", "nullness"}) // go/futurereturn-lsc
        ListenableScheduledFuture<?> future = mExecutor.schedule(mMockRunnable, 5, SECONDS);
        verifyNoInteractions(mMockRunnable);
        assertFalse(future.isDone());

        shadowOf(Looper.getMainLooper()).idle();
        assertFalse(future.isDone());
        verifyNoInteractions(mMockRunnable);

        advanceTimeBy(5 * 1000);
        assertTrue(future.isDone());
        verify(mMockRunnable).run();
    }

    @Test
    public void schedule_executesCallableImmediately() throws Exception {
        Object returnValue = new Object();
        when(mMockCallable.call()).thenReturn(returnValue);
        ListenableScheduledFuture<Object> future = mExecutor.schedule(mMockCallable, 0, SECONDS);
        verifyNoInteractions(mMockCallable);
        assertFalse(future.isDone());

        shadowOf(Looper.getMainLooper()).idle();
        assertEquals(returnValue, future.get());
    }

    @Test
    public void schedule_executesCallableWithDelay() throws Exception {
        Object returnValue = new Object();
        when(mMockCallable.call()).thenReturn(returnValue);
        ListenableScheduledFuture<Object> future = mExecutor.schedule(mMockCallable, 5, SECONDS);
        verifyNoInteractions(mMockCallable);
        assertFalse(future.isDone());

        shadowOf(Looper.getMainLooper()).idle();
        verifyNoInteractions(mMockCallable);
        assertFalse(future.isDone());

        advanceTimeBy(5 * 1000);
        assertTrue(future.isDone());
        assertEquals(returnValue, future.get());
    }

    @Test
    public void scheduleAtFixedRate() {
        ListenableScheduledFuture<?> future =
                mExecutor.scheduleAtFixedRate(mMockRunnable, 5, 10, SECONDS);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mMockRunnable, never()).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(2)).run();

        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(3)).run();

        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(4)).run();

        future.cancel(true);
        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(4)).run();
    }

    @Test
    public void scheduleAtFixedRate_slowRunnable() {
        var unused = mExecutor.scheduleAtFixedRate(mSlowRunnable, 5, 10, SECONDS);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mSlowRunnable, never()).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(1)).run();

        advanceTimeBy(10 * 1000);
        verify(mSlowRunnable, times(2)).run();

        advanceTimeBy(10 * 1000);
        verify(mSlowRunnable, times(3)).run();
    }

    @Test
    public void scheduleWithFixedDelay() {
        ListenableScheduledFuture<?> future =
                mExecutor.scheduleWithFixedDelay(mMockRunnable, 5, 10, SECONDS);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mMockRunnable, never()).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mMockRunnable, times(2)).run();

        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(3)).run();

        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(4)).run();

        future.cancel(true);
        advanceTimeBy(10 * 1000);
        verify(mMockRunnable, times(4)).run();
    }

    @Test
    public void scheduleWithFixedDelay_slowRunnable() {
        var unused = mExecutor.scheduleWithFixedDelay(mSlowRunnable, 5, 10, SECONDS);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mSlowRunnable, never()).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(1)).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(2)).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(2)).run();

        advanceTimeBy(5 * 1000);
        verify(mSlowRunnable, times(3)).run();
    }
}
