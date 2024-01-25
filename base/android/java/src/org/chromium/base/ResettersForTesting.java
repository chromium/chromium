// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;

import org.chromium.build.BuildConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;

/**
 * ResettersForTesting provides functionality for reset values set for testing. This class is used
 * directly by test runners, but lives in prod code to simplify usage.
 *
 * It is required to invoke {@link #register(Runnable)} whenever a method called `set*ForTesting`,
 * such `setFooForTesting(Foo foo)` is invoked. Typical usage looks like this:
 *
 * <code>
 * class MyClass {
 *     private static MyClass sInstance;
 *
 *     public static MyClass getInstance() {
 *         if (sInstance == null) sInstance = new MyClass();
 *         return sInstance;
 *     }
 *
 *     public static void setMyClassForTesting(MyClass myClassObj) {
 *         var oldInstance = sInstance
 *         sInstance = myClassObj;
 *         ResettersForTesting.register(() -> sInstance = oldInstance);
 *     }
 * }
 * </code>
 *
 * This is not only used for singleton instances, but can also be used for resetting other static
 * members.
 *
 * <code>
 * class NeedsFoo {
 *     private static Foo sFooForTesting;
 *
 *     public void doThing() {
 *         Foo foo = sFooForTesting != null ? sFooForTesting : new FooImpl();
 *         foo.doItsThing();
 *     }
 *
 *     public static void setFooForTesting(Foo foo) {
 *         sFooForTesting = foo;
 *         ResettersForTesting.register(() -> sFooForTesting = null);
 *     }
 * }
 * </code>
 *
 * For cases where it is important that a particular resetter runs only once, even if the
 * `set*ForTesting` method is invoked multiple times, there is another variation that can be used.
 * In particular, since a lambda always ends up creating a new instance in Chromium builds, we can
 * avoid this by having a single static instance of the resetter, like this:
 *
 * <code>
 * private static class NeedsFooSingleDestroy {
 *     private static final class LazyHolder {
 *         private static Foo INSTANCE = new Foo();
 *     }
 *
 *     private static LazyHolder sFoo;
 *
 *     private static Runnable sOneShotResetter = () -> {
 *         sFoo.INSTANCE.destroy();
 *         sFoo = new Foo();
 *     };
 *
 *     public static void setFooForTesting(Foo foo) {
 *         sFoo.INSTANCE = foo;
 *         ResettersForTesting.register(sResetter);
 *     }
 * }
 * </code>
 */
public class ResettersForTesting {

    @IntDef({
        State.NOT_ENABLED,
        State.BETWEEN_TESTS,
        State.CLASS_SCOPED,
        State.METHOD_SCOPED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        // Ignore calls to register() if the class has never been initialized (e.g. browsertests).
        int NOT_ENABLED = 0; // Default state. Call enable() to move to BETWEEN_TESTS.
        // Calls to register() result in IllegalStateException.
        int BETWEEN_TESTS = 1; // Also the state for before/after all tests.
        // Calls to register() use sClassResetters.
        int CLASS_SCOPED = 2;
        // Calls to register() use sMethodResetters.
        int METHOD_SCOPED = 3;
    }

    private static final Object sLock = BuildConfig.IS_FOR_TEST ? new Object() : null;

    // LinkedHashSet is a set that provides ordering and enables one-shot resetters to only be
    // invoked once. For example, the following `sResetter` will only be in the set a single time.
    // <code>
    // private static final Runnable sResetter = () -> { ... }
    // ...
    // ResettersForTesting.register(sResetter);
    // </code>
    @GuardedBy("sLock")
    private static LinkedHashSet<Runnable> sClassResetters;

    @GuardedBy("sLock")
    private static LinkedHashSet<Runnable> sMethodResetters;

    @GuardedBy("sLock")
    private static @State int sState = State.NOT_ENABLED;

    @GuardedBy("sLock")
    private static boolean sIsFlushing;

    /**
     * Register a {@link Runnable} that will automatically execute during test tear down.
     * @param runnable the {@link Runnable} to execute.
     */
    public static void register(Runnable runnable) {
        // Allow calls from non-test code without callers needing to add a BuildConfig.IS_FOR_TEST
        // check (enables R8 to optimize away the call).
        if (!BuildConfig.IS_FOR_TEST) {
            return;
        }
        synchronized (sLock) {
            if (sIsFlushing) {
                throw new IllegalStateException(
                        "ResettersForTesting.register() called from within a resetting callback.");
            }
            switch (sState) {
                case State.NOT_ENABLED -> {}
                case State.BETWEEN_TESTS -> throw new IllegalStateException(
                        "ResettersForTesting.register() called when no test is active");
                case State.CLASS_SCOPED -> sClassResetters.add(runnable);
                case State.METHOD_SCOPED -> sMethodResetters.add(runnable);
            }
        }
    }

    /**
     * Execute and clear all the currently registered resetters.
     *
     * <p>This is not intended to be invoked manually, but is intended to be invoked by the test
     * runners automatically during tear down.
     */
    @GuardedBy("sLock")
    private static void flushResetters(LinkedHashSet<Runnable> activeSet) {
        assert !sIsFlushing : "Re-entrancy detected in ResettersForTesting";
        ArrayList<Runnable> resetters = new ArrayList<>(activeSet);
        activeSet.clear();

        // Ensure that resetters are run in reverse order, enabling nesting of values as well as
        // being more similar to C++ destruction order.
        Collections.reverse(resetters);

        sIsFlushing = true;
        try {
            for (Runnable resetter : resetters) {
                resetter.run();
            }
        } finally {
            sIsFlushing = false;
        }
    }

    /** Called by test runners before @BeforeClass methods. */
    public static void beforeClassHooksWillExecute() {
        synchronized (sLock) {
            assert sState == State.BETWEEN_TESTS : "Invalid state transition from state " + sState;
            sState = State.CLASS_SCOPED;
        }
    }

    /** Called by test runners after @BeforeClass methods, but before @Before methods. */
    public static void beforeHooksWillExecute() {
        synchronized (sLock) {
            assert sState == State.CLASS_SCOPED : "Invalid state transition from state " + sState;
            sState = State.METHOD_SCOPED;
        }
    }

    /** Called by test runners after @After methods. */
    public static void afterHooksDidExecute() {
        synchronized (sLock) {
            assert sState == State.METHOD_SCOPED : "Invalid state transition from state " + sState;
            sState = State.CLASS_SCOPED;
            flushResetters(sMethodResetters);
        }
    }

    /** Called by test runners after @AfterClass methods. */
    public static void afterClassHooksDidExecute() {
        synchronized (sLock) {
            assert sState == State.CLASS_SCOPED : "Invalid state transition from state " + sState;
            sState = State.BETWEEN_TESTS;
            flushResetters(sClassResetters);
        }
    }

    /** Enables calls to register(). */
    public static void enable() {
        assert BuildConfig.IS_FOR_TEST;
        synchronized (sLock) {
            assert sState == State.NOT_ENABLED;
            sState = State.BETWEEN_TESTS;
            sMethodResetters = new LinkedHashSet<>();
            sClassResetters = new LinkedHashSet<>();
        }
    }
}
