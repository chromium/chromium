// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;
import org.robolectric.DefaultTestLifecycle;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.TestLifecycle;
import org.robolectric.internal.SandboxTestRunner;
import org.robolectric.internal.bytecode.Sandbox;

import org.chromium.base.test.util.DisabledTest;

import java.lang.reflect.Method;

/**
 * A Robolectric Test Runner that configures Chromium-specific settings and initializes base
 * globals. If initializing base globals is not desired, then {@link
 * org.robolectric.RobolectricTestRunner} could be used directly.
 */
public class BaseRobolectricTestRunner extends RobolectricTestRunner {
    /** Tracks whether tests pass / fail for use in BaseTestLifecycle. */
    protected static class HelperTestRunner extends RobolectricTestRunner.HelperTestRunner {
        public static boolean sTestFailed;

        public HelperTestRunner(Class bootstrappedTestClass) throws InitializationError {
            super(bootstrappedTestClass);
        }

        @Override
        protected Statement methodBlock(final FrameworkMethod method) {
            Statement orig = super.methodBlock(method);
            return new Statement() {
                // Does not run the sandbox classloader.
                // Called after Lifecycle.beforeTest(), and before Lifecycle.afterTest().
                @Override
                public void evaluate() throws Throwable {
                    try {
                        orig.evaluate();
                    } catch (Throwable t) {
                        sTestFailed = true;
                        throw t;
                    }
                    sTestFailed = false;
                }
            };
        }
    }

    /** Before / after hooks that run in the context of the sandbox classloader. */
    public static class BaseTestLifecycle extends DefaultTestLifecycle {
        @Override
        public void beforeTest(Method method) {
            BaseRobolectricTestRule.setUp(method);
            super.beforeTest(method);
        }

        @Override
        public void afterTest(Method method) {
            super.afterTest(method);
            BaseRobolectricTestRule.tearDown(HelperTestRunner.sTestFailed);
        }
    }

    private static ClassLoader sSandboxClassLoader;

    public BaseRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    @Override
    protected SandboxTestRunner.HelperTestRunner getHelperTestRunner(Class bootstrappedTestClass) {
        try {
            return new HelperTestRunner(bootstrappedTestClass);
        } catch (InitializationError initializationError) {
            throw new RuntimeException(initializationError);
        }
    }

    @Override
    protected Class<? extends TestLifecycle> getTestLifecycleClass() {
        return BaseTestLifecycle.class;
    }

    @Override
    protected void beforeTest(Sandbox sandbox, FrameworkMethod method, Method bootstrappedMethod)
            throws Throwable {
        super.beforeTest(sandbox, method, bootstrappedMethod);

        // Our test runner is designed to require only one Sandbox per-process.
        var actualClassLoader = sandbox.getRobolectricClassLoader();
        var expectedClassLoader = sSandboxClassLoader;
        if (expectedClassLoader == null) {
            sSandboxClassLoader = actualClassLoader;
        } else if (actualClassLoader != expectedClassLoader) {
            throw new RuntimeException("Invalid test batch detected. https://crbug.com/1465376");
        }
    }

    @Override
    protected boolean isIgnored(FrameworkMethod method) {
        if (super.isIgnored(method) || method.getAnnotation(DisabledTest.class) != null) {
            return true;
        }
        Class<?> testSuiteClass = method.getDeclaringClass();
        return testSuiteClass.getAnnotation(DisabledTest.class) != null;
    }
}
