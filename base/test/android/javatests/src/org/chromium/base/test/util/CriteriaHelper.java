// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Handler;
import android.os.Looper;

import org.hamcrest.Matchers;

import org.chromium.base.ThreadUtils;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helper methods for creating and managing criteria.
 *
 * <p>
 * If possible, use callbacks or testing delegates instead of criteria as they
 * do not introduce any polling delays.  Should only use criteria if no suitable
 * other approach exists.
 *
 * <p>
 * The Runnable variation of the CriteriaHelper methods allows a flexible way of verifying any
 * number of conditions are met prior to proceeding.
 *
 * <pre>
 * Example:
 * <code>
 * private void verifyMenuShown() {
 *     CriteriaHelper.pollUiThread(() -> {
 *         Criteria.checkThat("App menu was null", getActivity().getAppMenuHandler(),
 *                 Matchers.notNullValue());
 *         Criteria.checkThat("App menu was not shown",
 *                 getActivity().getAppMenuHandler().isAppMenuShowing(), Matchers.is(true));
 *     });
 * }
 * </code>
 * </pre>
 *
 * <p>
 * To verify simple conditions, the Callback variation can be less verbose.
 *
 * <pre>
 * Example:
 * <code>
 * private void assertMenuShown() {
 *     CriteriaHelper.pollUiThread(() -> getActivity().getAppMenuHandler().isAppMenuShowing(),
 *             "App menu was not shown");
 * }
 * </code>
 * </pre>
 */
public class CriteriaHelper {
    /** The default maximum time to wait for a criteria to become valid. */
    public static final long DEFAULT_MAX_TIME_TO_POLL = 3000L;
    /** The default polling interval to wait between checking for a satisfied criteria. */
    public static final long DEFAULT_POLLING_INTERVAL = 50;

    private static final long DEFAULT_JUNIT_MAX_TIME_TO_POLL = 1000;
    private static final long DEFAULT_JUNIT_POLLING_INTERVAL = 1;

    /**
     * Checks whether the given Runnable completes without exception at a given interval, until
     * either the Runnable successfully completes, or the maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Criteria on the Instrumentation thread, which more often than not is not
     * correct in an InstrumentationTest. Use
     * {@link #pollUiThread(Runnable, long, long)} instead.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(
            Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        assert !ThreadUtils.runningOnUiThread();
        pollThreadInternal(criteria, maxTimeoutMs, checkIntervalMs, false);
    }

    private static void pollThreadInternal(
            Runnable criteria, long maxTimeoutMs, long checkIntervalMs, boolean shouldNest) {
        Throwable throwable;
        try {
            criteria.run();
            return;
        } catch (Throwable e) {
            // Espresso catches, wraps, and re-throws the exception we want the CriteriaHelper
            // to catch.
            if (e instanceof CriteriaNotSatisfiedException
                    || e.getCause() instanceof CriteriaNotSatisfiedException) {
                throwable = e;
            } else {
                throw e;
            }
        }
        TimeoutTimer timer = new TimeoutTimer(maxTimeoutMs);
        while (!timer.isTimedOut()) {
            if (shouldNest) {
                nestThread(checkIntervalMs);
            } else {
                sleepThread(checkIntervalMs);
            }
            try {
                criteria.run();
                return;
            } catch (Throwable e) {
                if (e instanceof CriteriaNotSatisfiedException
                        || e.getCause() instanceof CriteriaNotSatisfiedException) {
                    throwable = e;
                } else {
                    throw e;
                }
            }
        }
        throw new AssertionError(throwable);
    }

    private static void sleepThread(long checkIntervalMs) {
        try {
            Thread.sleep(checkIntervalMs);
        } catch (InterruptedException e) {
            // Catch the InterruptedException. If the exception occurs before maxTimeoutMs
            // and the criteria is not satisfied, the while loop will run again.
        }
    }

    private static void nestThread(long checkIntervalMs) {
        AtomicBoolean called = new AtomicBoolean(false);

        // Ensure we pump the message handler in case no new tasks arrive.
        new Handler(Looper.myLooper()).postDelayed(() -> { called.set(true); }, checkIntervalMs);

        TimeoutTimer timer = new TimeoutTimer(checkIntervalMs);
        // To allow a checkInterval of 0ms, ensure we at least run a single task, which allows a
        // test to check conditions between each task run on the thread.
        do {
            try {
                LooperUtils.runSingleNestedLooperTask();
            } catch (IllegalArgumentException | IllegalAccessException | SecurityException
                    | InvocationTargetException e) {
                throw new RuntimeException(e);
            }
        } while (!timer.isTimedOut() && !called.get());
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval.
     *
     * <p>
     * This evaluates the Runnable on the test thread, which more often than not is not correct
     * in an InstrumentationTest.  Use {@link #pollUiThread(Runnable)} instead.
     *
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Criteria, long, long)
     */
    public static void pollInstrumentationThread(Runnable criteria) {
        pollInstrumentationThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> criteria,
            String failureReason, long maxTimeoutMs, long checkIntervalMs) {
        pollInstrumentationThread(
                toNotSatisfiedRunnable(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollInstrumentationThread(criteria, null, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria, String failureReason) {
        pollInstrumentationThread(
                criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria) {
        pollInstrumentationThread(criteria, null);
    }

    /**
     * Checks whether the given Runnable completes without exception at a given interval on the UI
     * thread, until either the Runnable successfully completes, or the maxTimeoutMs number of ms
     * has elapsed.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThread(
            final Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        assert !ThreadUtils.runningOnUiThread();
        pollInstrumentationThread(() -> {
            AtomicReference<Throwable> throwableRef = new AtomicReference<>();
            ThreadUtils.runOnUiThreadBlocking(() -> {
                try {
                    criteria.run();
                } catch (Throwable t) {
                    throwableRef.set(t);
                }
            });
            Throwable throwable = throwableRef.get();
            if (throwable != null) {
                if (throwable instanceof CriteriaNotSatisfiedException) {
                    throw new CriteriaNotSatisfiedException(throwable);
                } else if (throwable instanceof RuntimeException) {
                    throw(RuntimeException) throwable;
                } else {
                    throw new RuntimeException(throwable);
                }
            }
        }, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval on
     * the UI thread.
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThread(final Runnable criteria) {
        pollUiThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason,
            long maxTimeoutMs, long checkIntervalMs) {
        pollUiThread(
                toNotSatisfiedRunnable(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollUiThread(criteria, null, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread. A static failure reason is given.
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason) {
        pollUiThread(criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria) {
        pollUiThread(criteria, null);
    }

    /**
     * Checks whether the given Runnable completes without exception at a given interval on the UI
     * thread, until either the Runnable successfully completes, or the maxTimeoutMs number of ms
     * has elapsed.
     * This call will nest the Looper in order to wait for the Runnable to complete.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThreadNested(
            Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        assert ThreadUtils.runningOnUiThread();
        pollThreadInternal(criteria, maxTimeoutMs, checkIntervalMs, true);
    }

    /**
     * Checks whether the given Runnable is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     * This call will nest the Looper in order to wait for the Criteria to be satisfied.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadNested(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollUiThreadNested(toNotSatisfiedRunnable(criteria, null), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval on
     * the UI thread. This call will nest the Looper in order to wait for the Runnable to complete.
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThreadNested(final Runnable criteria) {
        pollUiThreadNested(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread. This call will nest the Looper in order to wait for the Criteria to be satisfied.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadNested(final Callable<Boolean> criteria) {
        pollUiThreadNested(toNotSatisfiedRunnable(criteria, null));
    }

    /**
     * Sleeps the JUnit UI thread to wait on the condition. The condition must be met by a
     * background thread that does not block on the UI thread.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadForJUnit(final Callable<Boolean> criteria) {
        pollUiThreadForJUnit(toNotSatisfiedRunnable(criteria, null));
    }

    /**
     * Sleeps the JUnit UI thread to wait on the criteria. The criteria must be met by a
     * background thread that does not block on the UI thread.
     *
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadForJUnit(final Runnable criteria) {
        assert ThreadUtils.runningOnUiThread();
        pollThreadInternal(
                criteria, DEFAULT_JUNIT_MAX_TIME_TO_POLL, DEFAULT_JUNIT_POLLING_INTERVAL, false);
    }

    private static Runnable toNotSatisfiedRunnable(
            Callable<Boolean> criteria, String failureReason) {
        return () -> {
            boolean isSatisfied;
            try {
                isSatisfied = criteria.call();
            } catch (RuntimeException re) {
                throw re;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            Criteria.checkThat(failureReason, isSatisfied, Matchers.is(true));
        };
    }
}
