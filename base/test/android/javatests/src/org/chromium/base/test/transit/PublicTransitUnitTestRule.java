// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertThrows;

import org.junit.function.ThrowingRunnable;
import org.junit.rules.ExternalResource;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Rule for Robolectric unit tests of Public Transit.
 *
 * <p>Transitions must not run on the UI thread, which is the test thread in Robolectric, so this
 * runs them in a separate thread and rethrows their exceptions in the test thread. After each test,
 * stops transitions still running and hops off Public Transit.
 */
@NullMarked
public class PublicTransitUnitTestRule extends ExternalResource {
    private final List<AsyncTransition> mTransitions = new ArrayList<>();

    @Override
    protected void after() {
        for (AsyncTransition transition : mTransitions) {
            transition.stop();
        }
        mTransitions.clear();
        TrafficControl.hopOffPublicTransit();
    }

    /** Runs |transition| in a separate thread and waits for it to finish. */
    public void runTransition(ThrowingRunnable transition) throws Throwable {
        startTransition(transition).join();
    }

    /** Runs |transition| in a separate thread and returns the exception it throws. */
    public <T extends Throwable> T runTransitionExpectingThrow(
            Class<T> expectedThrowable, ThrowingRunnable transition) {
        return assertThrows(expectedThrowable, () -> runTransition(transition));
    }

    /** Starts |transition| in a separate thread so the test can interact with it while it runs. */
    public AsyncTransition startTransition(ThrowingRunnable transition) {
        AsyncTransition asyncTransition = new AsyncTransition(transition);
        mTransitions.add(asyncTransition);
        return asyncTransition;
    }

    /** A transition running in a separate thread. */
    public static class AsyncTransition {
        private final Thread mThread;
        private final AtomicReference<Throwable> mThrowable = new AtomicReference<>();

        private AsyncTransition(ThrowingRunnable transition) {
            mThread =
                    new Thread(
                            () -> {
                                try {
                                    transition.run();
                                } catch (Throwable t) {
                                    mThrowable.set(t);
                                }
                            });
            mThread.start();
        }

        /** Waits for the transition to finish and rethrows its exception, if any. */
        public void join() throws Throwable {
            mThread.join();
            Throwable t = mThrowable.get();
            if (t != null) {
                throw t;
            }
        }

        private void stop() {
            mThread.interrupt();
            try {
                mThread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}
