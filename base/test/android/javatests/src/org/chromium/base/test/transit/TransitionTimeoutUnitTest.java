// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;

import static org.chromium.base.test.transit.Triggers.runTo;

import com.google.errorprone.annotations.CheckReturnValue;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit Tests for per-Condition/Transition timeouts. */
@RunWith(BaseRobolectricTestRunner.class)
public class TransitionTimeoutUnitTest {
    private static final List<Thread> sCreatedThreads =
            Collections.synchronizedList(new ArrayList<>());

    @After
    public void tearDown() {
        TrafficControl.hopOffPublicTransit();
        synchronized (sCreatedThreads) {
            for (Thread thread : sCreatedThreads) {
                if (thread.isAlive()) {
                    thread.interrupt();
                }
            }
            sCreatedThreads.clear();
        }
    }

    @Test
    public void testCalculateTimeoutMs_noTimeouts() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.TransitionOptions.DEFAULT,
                        List.of(new SettableCondition("default")));
        assertThat(timeout).isEqualTo(ConditionWaiter.MAX_TIME_TO_POLL);
    }

    @Test
    public void testCalculateTimeoutMs_customTimeoutAndDefaultTimeout() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.TransitionOptions.DEFAULT,
                        List.of(
                                new SettableCondition("default"),
                                new SettableCondition("short").withTimeout(500)));
        assertThat(timeout).isEqualTo(5000L);
    }

    @Test
    public void testCalculateTimeoutMs_customTimeoutOnly() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.TransitionOptions.DEFAULT,
                        List.of(new SettableCondition("short").withTimeout(500)));
        assertThat(timeout).isEqualTo(500L);
    }

    @Test
    public void testCalculateTimeoutMs_customTimeoutGreaterThanSmartTimeoutMinimum() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.TransitionOptions.DEFAULT,
                        List.of(
                                new SettableCondition("default"),
                                new SettableCondition("short").withTimeout(500),
                                new SettableCondition("long").withTimeout(5000)));
        assertThat(timeout).isEqualTo(5000L);
    }

    @Test
    public void testCalculateTimeoutMs_transitionTimeoutOnly() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.newOptions().withTimeout(1000).build(),
                        List.of(new SettableCondition("default")));
        assertThat(timeout).isEqualTo(1000L);
    }

    @Test
    public void testCalculateTimeoutMs_transitionTimeoutGreaterThanCustomTimeout() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.newOptions().withTimeout(3000).build(),
                        List.of(
                                new SettableCondition("default"),
                                new SettableCondition("short").withTimeout(500)));
        assertThat(timeout).isEqualTo(3000L);
    }

    @Test
    public void testCalculateTimeoutMs_transitionTimeoutLessThanCustomTimeout() {
        long timeout =
                ConditionWaiter.calculateTimeoutMs(
                        Transition.newOptions().withTimeout(1000).build(),
                        List.of(
                                new SettableCondition("default"),
                                new SettableCondition("long").withTimeout(5000)));
        assertThat(timeout).isEqualTo(5000L);
    }

    @Test
    public void testZeroTimeout_fulfilledImmediately() throws Throwable {
        SettableCondition zeroTimeoutCondition =
                new SettableCondition("zero timeout condition").withTimeout(0);

        // Should succeed immediately
        long startTime = ConditionWaiter.getNow();
        expectTransitionSuccess(runTo(zeroTimeoutCondition.setFulfilled()), zeroTimeoutCondition);
        long duration = ConditionWaiter.getNow() - startTime;
        assertWithMessage("Should complete quickly, took %sms", duration)
                .that(duration)
                .isLessThan(500L);
    }

    @Test
    public void testZeroTimeout_fulfilledLate_fails() throws Throwable {
        SettableCondition zeroTimeoutCondition =
                new SettableCondition("zero timeout condition").withTimeout(0);

        TravelException e =
                expectTransitionFailure(
                        runTo(zeroTimeoutCondition.setFulfilledAfterDelay(500)).withTimeout(200),
                        zeroTimeoutCondition);
        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("[FAIL] zero timeout condition");
        assertThat(e).hasMessageThat().contains("timeout was 0 ms");
    }

    @Test
    public void testZeroTimeout_fulfilledOnFirstCheck_succeeds() throws Throwable {
        SettableCondition zeroTimeoutCondition =
                new SettableCondition("zero timeout condition").withTimeout(0);

        expectTransitionSuccess(
                runTo(zeroTimeoutCondition.setFulfilledAfterBlockingDelay(100)),
                zeroTimeoutCondition);
    }

    @Test
    public void testZeroTimeout_neverFulfilled_fails() throws Throwable {
        SettableCondition zeroTimeoutCondition =
                new SettableCondition("zero timeout condition").withTimeout(0);

        TravelException e =
                expectTransitionFailure(Triggers.noopTo().withTimeout(500), zeroTimeoutCondition);
        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("[FAIL] zero timeout condition");
        assertThat(e).hasMessageThat().contains("timeout was 0 ms");
    }

    @Test
    public void testCustomTimeout_fulfilledWithinTimeout() throws Throwable {
        SettableCondition customTimeoutCondition =
                new SettableCondition("custom timeout condition").withTimeout(1000);

        expectTransitionSuccess(
                runTo(customTimeoutCondition.setFulfilledAfterDelay(500)), customTimeoutCondition);
    }

    @Test
    public void testCustomTimeout_fulfilledAfterTimeout_fails() throws Throwable {
        SettableCondition customTimeoutCondition =
                new SettableCondition("custom timeout condition").withTimeout(200);

        TravelException e =
                expectTransitionFailure(
                        runTo(customTimeoutCondition.setFulfilledAfterDelay(500)).withTimeout(1000),
                        customTimeoutCondition);
        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("[FAIL] custom timeout condition");
        assertThat(e).hasMessageThat().contains("timeout was 200 ms");
    }

    @Test
    public void testTransitionTimeout_neverFulfilled_fails() throws Throwable {
        SettableCondition neverFulfilledCondition =
                new SettableCondition("never fulfilled condition");

        TravelException e =
                expectTransitionFailure(runTo(() -> {}).withTimeout(200), neverFulfilledCondition);
        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().doesNotContain("timeout was");
    }

    @Test
    public void testTransitionTimeout_fulfilledLate_fails() throws Throwable {
        SettableCondition defaultTimeoutCondition =
                new SettableCondition("default timeout condition");

        TravelException e =
                expectTransitionFailure(
                        runTo(defaultTimeoutCondition.setFulfilledAfterDelay(500)).withTimeout(200),
                        defaultTimeoutCondition);
        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().doesNotContain("timeout was");
    }

    @Test
    public void testTransitionTimeout_fulfilledWithinTimeout_succeeds() throws Throwable {
        SettableCondition fulfilledWithinTimeoutCondition =
                new SettableCondition("fulfilled within timeout condition");

        expectTransitionSuccess(
                runTo(fulfilledWithinTimeoutCondition.setFulfilledAfterDelay(100)).withTimeout(300),
                fulfilledWithinTimeoutCondition);
    }

    @Test
    public void testTransitionTimeout_fulfilledImmediately_succeeds() throws Throwable {
        SettableCondition fulfilledImmediatelyCondition =
                new SettableCondition("fulfilled immediately condition");

        expectTransitionSuccess(
                runTo(fulfilledImmediatelyCondition.setFulfilled()).withTimeout(300),
                fulfilledImmediatelyCondition);
    }

    @Test
    public void testPerConditionTimeout_doesNotAbortEarly_BFulfilled() throws Throwable {
        SettableCondition shortTimeoutCondition =
                new SettableCondition("short timeout, never fulfills").withTimeout(200);
        SettableCondition longTimeoutCondition =
                new SettableCondition("long timeout, fulfills late").withTimeout(1000);

        long startTime = ConditionWaiter.getNow();
        TravelException e =
                expectTransitionFailure(
                        runTo(longTimeoutCondition.setFulfilledAfterDelay(500)),
                        shortTimeoutCondition,
                        longTimeoutCondition);
        long duration = ConditionWaiter.getNow() - startTime;

        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("short timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("[OK  ] long timeout, fulfills late");
        assertThat(e).hasMessageThat().contains("[FAIL] short timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("timeout was 200 ms");

        assertWithMessage("Should wait for B to fulfill, took %sms", duration)
                .that(duration)
                .isAtLeast(500L);
        assertWithMessage("Should not wait full 1000ms, took %sms", duration)
                .that(duration)
                .isLessThan(800L);
    }

    @Test
    public void testPerConditionTimeout_errorDoesNotAbortEarly_BFulfilled() throws Throwable {
        SettableCondition shortTimeoutCondition =
                new SettableCondition("short timeout, error").withTimeout(200);
        shortTimeoutCondition.setStatus(Condition.error("some error"));
        SettableCondition longTimeoutCondition =
                new SettableCondition("long timeout, fulfills late").withTimeout(1000);

        long startTime = ConditionWaiter.getNow();
        TravelException e =
                expectTransitionFailure(
                        runTo(longTimeoutCondition.setFulfilledAfterDelay(500)),
                        shortTimeoutCondition,
                        longTimeoutCondition);
        long duration = ConditionWaiter.getNow() - startTime;

        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("short timeout, error");
        assertThat(e).hasMessageThat().contains("[OK  ] long timeout, fulfills late");
        assertThat(e).hasMessageThat().contains("[ERR*] short timeout, error");

        assertWithMessage("Should wait for B to fulfill, took %sms", duration)
                .that(duration)
                .isAtLeast(500L);
        assertWithMessage("Should not wait full 1000ms, took %sms", duration)
                .that(duration)
                .isLessThan(800L);
    }

    @Test
    public void testPerConditionTimeout_doesNotAbortEarly_neverFulfilled() throws Throwable {
        SettableCondition shortTimeoutCondition =
                new SettableCondition("short timeout, never fulfills").withTimeout(200);
        SettableCondition longTimeoutCondition =
                new SettableCondition("long timeout, never fulfills").withTimeout(1000);

        long startTime = ConditionWaiter.getNow();
        TravelException e =
                expectTransitionFailure(
                        Triggers.noopTo(), shortTimeoutCondition, longTimeoutCondition);
        long duration = ConditionWaiter.getNow() - startTime;

        assertThat(e).hasMessageThat().contains("Did not complete");
        assertThat(e).hasMessageThat().contains("short timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("long timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("[FAIL] short timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("[FAIL] long timeout, never fulfills");
        assertThat(e).hasMessageThat().contains("timeout was 200 ms");
        assertThat(e).hasMessageThat().contains("timeout was 1000 ms");

        assertWithMessage("Should wait for B to timeout, took %sms", duration)
                .that(duration)
                .isAtLeast(1000L);
    }

    private static class SettableCondition extends Condition {
        private final String mDescription;
        private ConditionStatus mStatusTemplate = Condition.notFulfilled("Initial state");

        SettableCondition(String description) {
            super(/* isRunOnUiThread= */ false);
            mDescription = description;
        }

        @Override
        public SettableCondition withTimeout(@Nullable Integer timeoutMs) {
            super.withTimeout(timeoutMs);
            return this;
        }

        @CheckReturnValue
        private Runnable setFulfilled() {
            return () -> setStatus(fulfilled());
        }

        @CheckReturnValue
        private Runnable setFulfilledAfterDelay(long delayMs) {
            return () -> {
                Thread thread =
                        new Thread(
                                () -> {
                                    try {
                                        Thread.sleep(delayMs);
                                    } catch (InterruptedException e) {
                                        return;
                                    }
                                    setStatus(fulfilled());
                                });
                sCreatedThreads.add(thread);
                thread.start();
            };
        }

        @CheckReturnValue
        private Runnable setFulfilledAfterBlockingDelay(long delayMs) {
            return () -> {
                try {
                    Thread.sleep(delayMs);
                } catch (InterruptedException e) {
                    return;
                }
                setStatus(fulfilled());
            };
        }

        public void setStatus(ConditionStatus status) {
            mStatusTemplate = status;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            if (mStatusTemplate.isFulfilled()) {
                return fulfilled(mStatusTemplate.getMessage());
            } else if (mStatusTemplate.isError()) {
                return error(mStatusTemplate.getMessage());
            } else if (mStatusTemplate.isAwaiting()) {
                return awaiting(mStatusTemplate.getMessage());
            } else {
                return notFulfilled(mStatusTemplate.getMessage());
            }
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }

    private static TravelException expectTransitionFailure(
            TripBuilder builder, Condition... conditions) throws Throwable {
        State destinationState = new State("destination state");
        for (Condition c : conditions) {
            destinationState.declareEnterCondition(c);
        }
        AtomicReference<TravelException> exceptionRef = new AtomicReference<>();
        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            TravelException e =
                                    assertThrows(
                                            TravelException.class,
                                            () -> {
                                                builder.enterState(destinationState);
                                            });
                            exceptionRef.set(e);
                        });
        transition.joinAndVerify();
        return exceptionRef.get();
    }

    private static void expectTransitionSuccess(TripBuilder builder, Condition... conditions)
            throws Throwable {
        State destinationState = new State("destination state");
        for (Condition c : conditions) {
            destinationState.declareEnterCondition(c);
        }
        AsyncTransition transition =
                new AsyncTransition(
                        () -> {
                            builder.enterState(destinationState);
                        });
        transition.joinAndVerify();
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
