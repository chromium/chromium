// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.BuildConfig;

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
    // LinkedHashSet is a set that provides ordering and enables one-shot resetters to only be
    // invoked once. For example, the following `sResetter` will only be in the set a single time.
    // <code>
    // private static final Runnable sResetter = () -> { ... }
    // ...
    // ResettersForTesting.register(sResetter);
    // </code>
    private static final LinkedHashSet<Runnable> sClassResetters =
            BuildConfig.IS_FOR_TEST ? new LinkedHashSet<>() : null;
    private static final LinkedHashSet<Runnable> sMethodResetters =
            BuildConfig.IS_FOR_TEST ? new LinkedHashSet<>() : null;
    // Starts in "class mode", since @BeforeClass runs before @Before.
    // Test runners toggle this via setMethodMode(), then reset it via onAfterClass().
    private static boolean sMethodMode;

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
        if (sMethodMode) {
            sMethodResetters.add(runnable);
        } else {
            sClassResetters.add(runnable);
        }
    }

    /**
     * Execute and clear all the currently registered resetters.
     *
     * This is not intended to be invoked manually, but is intended to be invoked by the test
     * runners automatically during tear down.
     */
    private static void flushResetters(LinkedHashSet activeSet) {
        ArrayList<Runnable> resetters = new ArrayList<>(activeSet);
        activeSet.clear();

        // Ensure that resetters are run in reverse order, enabling nesting of values as well as
        // being more similar to C++ destruction order.
        Collections.reverse(resetters);
        for (Runnable resetter : resetters) {
            resetter.run();
        }
    }

    /** Called by test runners after @After methods. */
    public static void onAfterMethod() {
        flushResetters(sMethodResetters);
    }

    /** Called by test runners after @AfterClass methods. */
    public static void onAfterClass() {
        flushResetters(sClassResetters);
        sMethodMode = false;
    }

    /** Called by test runners after @BeforeClass methods, but before @Before methods. */
    public static void setMethodMode() {
        sMethodMode = true;
    }
}
