// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import com.google.errorprone.annotations.CheckReturnValue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicReference;

/** Unit Tests for Transition timeouts. */
@RunWith(BaseRobolectricTestRunner.class)
public class TransitionTimeoutUnitTest {

    @Test
    public void testTransitionTimeout_neverFulfilled_fails() throws Throwable {
        SettableCondition condition = new SettableCondition("never fulfilled condition");
        State destinationState = new State("destination state");
        destinationState.declareEnterCondition(condition);

        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            TripBuilder builder = Triggers.runTo(() -> {}).withTimeout(200);
                            TravelException e =
                                    assertThrows(
                                            TravelException.class,
                                            () -> {
                                                builder.enterState(destinationState);
                                            });
                            assertTrue(e.getMessage().contains("Did not complete"));
                        });

        transition.joinAndVerify();
    }

    @Test
    public void testTransitionTimeout_fulfilledLate_fails() throws Throwable {
        SettableCondition condition = new SettableCondition("default timeout condition");

        State destinationState = new State("destination state");
        destinationState.declareEnterCondition(condition);

        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            TripBuilder builder =
                                    Triggers.runTo(condition.setFulfilledAfterDelay(500))
                                            .withTimeout(200);
                            TravelException e =
                                    assertThrows(
                                            TravelException.class,
                                            () -> {
                                                builder.enterState(destinationState);
                                            });
                            assertTrue(e.getMessage().contains("Did not complete"));
                        });

        transition.joinAndVerify();
    }

    @Test
    public void testTransitionTimeout_fulfilledWithinTimeout_succeeds() throws Throwable {
        SettableCondition condition = new SettableCondition("fulfilled within timeout condition");

        State destinationState = new State("destination state");
        destinationState.declareEnterCondition(condition);

        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            TripBuilder builder =
                                    Triggers.runTo(condition.setFulfilledAfterDelay(100))
                                            .withTimeout(300);
                            builder.enterState(destinationState);
                        });

        transition.joinAndVerify();
    }

    @Test
    public void testTransitionTimeout_fulfilledImmediately_succeeds() throws Throwable {
        SettableCondition condition = new SettableCondition("fulfilled immediately condition");

        State destinationState = new State("destination state");
        destinationState.declareEnterCondition(condition);

        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            TripBuilder builder =
                                    Triggers.runTo(condition.setFulfilled()).withTimeout(300);
                            builder.enterState(destinationState);
                        });

        transition.joinAndVerify();
    }

    private static class SettableCondition extends Condition {
        private final String mDescription;
        private ConditionStatus mStatus = Condition.notFulfilled("Initial state");

        SettableCondition(String description) {
            super(/* isRunOnUiThread= */ false);
            mDescription = description;
        }

        @CheckReturnValue
        private Runnable setFulfilled() {
            return () -> setStatus(fulfilled());
        }

        @CheckReturnValue
        private Runnable setFulfilledAfterDelay(long delayMs) {
            return () -> {
                new Thread(
                                () -> {
                                    try {
                                        Thread.sleep(delayMs);
                                    } catch (InterruptedException e) {
                                        throw new RuntimeException(e);
                                    }
                                    setStatus(fulfilled());
                                })
                        .start();
            };
        }

        public void setStatus(ConditionStatus status) {
            mStatus = status;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return mStatus;
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }

    /**
     * Runs a transition in a background thread to allow manipulating conditions concurrently during
     * transition polling.
     */
    private static class AsyncTransition {
        private final Thread mThread;
        private final AtomicReference<Throwable> mException = new AtomicReference<>();

        AsyncTransition(Runnable transitionBlock) {
            mThread =
                    new Thread(
                            () -> {
                                try {
                                    transitionBlock.run();
                                } catch (Throwable t) {
                                    mException.set(t);
                                }
                            });
            mThread.start();
        }

        public void joinAndVerify() throws Throwable {
            mThread.join();
            Throwable t = mException.get();
            if (t != null) {
                throw t;
            }
        }
    }
}
