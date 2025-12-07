// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.runners.model.Statement;
import org.junit.runners.model.TestTimedOutException;

import org.chromium.base.test.util.TimeoutTimer;

import java.util.concurrent.TimeUnit;

/**
 * Stricter version of Robolectric's {@link org.robolectric.internal.TimeLimitedStatement}.
 *
 * <p>If the timeout is reached, interrupts the test thread, which will continue running to the end
 * of the test. Whether the test fails or passes after handling the {@link InterruptedException},
 * throws {@link TestTimedOutException}.
 */
public class BaseTimeLimitedStatement extends Statement {

    private final long mTimeoutMs;
    private final Statement mDelegate;
    private volatile StackTraceElement[] mInterruptedStackTrace;

    public BaseTimeLimitedStatement(long timeoutMs, Statement delegate) {
        mTimeoutMs = timeoutMs;
        mDelegate = delegate;
    }

    @Override
    public void evaluate() throws Throwable {
        Thread testThread = Thread.currentThread();
        Thread timeoutThread =
                new Thread(
                        () -> {
                            try {
                                TimeoutTimer timeoutTimer = new TimeoutTimer(mTimeoutMs);
                                while (!timeoutTimer.isTimedOut()) {
                                    Thread.sleep(timeoutTimer.getRemainingMs());
                                }
                                mInterruptedStackTrace = testThread.getStackTrace();
                                testThread.interrupt();
                            } catch (InterruptedException e) {
                                // The test thread finished running the test before the timeout.
                            }
                        },
                        "Robolectric time-limited test");
        timeoutThread.start();

        try {
            mDelegate.evaluate();
        } catch (InterruptedException e) {
            // The test thread was interrupted, likely by the timeout thread, and the test thread
            // threw the InterruptedException, which got bubbled up to evaluate().
            Exception e2 = new TestTimedOutException(mTimeoutMs, TimeUnit.MILLISECONDS);
            e2.setStackTrace(e.getStackTrace());
            throw e2;
        } catch (Throwable e) {
            if (mInterruptedStackTrace != null) {
                // The test thread was interrupted (or was about to be interrupted) by the timeout
                // thread, and the test thread handled the InterruptedException, which is what
                // happens in most cases.
                //
                // Suppress the thrown exception (still will be printed in the log) and throw a
                // TestTimedOutException instead.
                Exception e2 = new TestTimedOutException(mTimeoutMs, TimeUnit.MILLISECONDS);
                e2.setStackTrace(mInterruptedStackTrace);
                e2.addSuppressed(e);
                throw e2;
            } else {
                // evaluate() threw an unrelated Throwable, so just rethrow it.
                throw e;
            }
        } finally {
            // Finish the timeout thread.
            timeoutThread.interrupt();
            try {
                timeoutThread.join();
            } catch (InterruptedException e) {
                // Ignore the exception and let the check below decide whether to throw a
                // TestTimedOutException.
            }

            // Clear the interrupted flag on the test thread in case the test thread was
            // interrupted. Ignore the result since |mInterruptedStackTrace| already tracks whether
            // the test thread was interrupted.
            Thread.interrupted();
        }

        // If the test thread was interrupted by the timeout thread, throw a TestTimedOutException.
        // The InterruptedException was handled, but the test should fail regardless.
        if (mInterruptedStackTrace != null) {
            TestTimedOutException e2 = new TestTimedOutException(mTimeoutMs, TimeUnit.MILLISECONDS);
            e2.setStackTrace(mInterruptedStackTrace);
            throw e2;
        }
    }
}
