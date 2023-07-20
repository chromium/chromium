// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;
import org.robolectric.DefaultTestLifecycle;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.TestLifecycle;
import org.robolectric.internal.SandboxTestRunner;
import org.robolectric.internal.bytecode.InstrumentationConfiguration;
import org.robolectric.internal.bytecode.Sandbox;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Flag;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.PathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.TimeoutTimer;

import java.lang.reflect.Method;

/**
 * A Robolectric Test Runner that configures Chromium-specific settings and initializes base
 * globals. If initializing base globals is not desired, then {@link
 * org.robolectric.RobolectricTestRunner} could be used directly.
 */
public class BaseRobolectricTestRunner extends RobolectricTestRunner {
    /**
     * Tracks whether tests pass / fail for use in BaseTestLifecycle.
     */
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

    /**
     * Before / after hooks that run in the context of the sandbox classloader.
     */
    public static class BaseTestLifecycle extends DefaultTestLifecycle {
        @Override
        public void beforeTest(Method method) {
            UmaRecorderHolder.setUpNativeUmaRecorder(false);
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
            ContextUtils.initApplicationContextForTests(
                    ApplicationProvider.getApplicationContext());
            ApplicationStatus.initialize(ApplicationProvider.getApplicationContext());
            UmaRecorderHolder.resetForTesting();
            CommandLineFlags.setUpClass(method.getDeclaringClass());
            CommandLineFlags.setUpMethod(method);
            BundleUtils.resetForTesting();
            Flag.resetAllInMemoryCachedValuesForTesting();
            super.beforeTest(method);
        }

        @Override
        public void afterTest(Method method) {
            try {
                // https://crbug.com/1392817 for context as to why we do this.
                PostTask.flushJobsAndResetForTesting();
            } catch (InterruptedException e) {
                HelperTestRunner.sTestFailed = true;
                throw new RuntimeException(e);
            } finally {
                CommandLineFlags.tearDownMethod();
                CommandLineFlags.tearDownClass();
                ResettersForTesting.onAfterMethod();
                ApplicationStatus.destroyForJUnitTests();
                ContextUtils.clearApplicationContextForTests();
                PathUtils.resetForTesting();
                ThreadUtils.clearUiThreadForTesting();
                super.afterTest(method);
                // Run assertions only when the test has not already failed so as to not mask
                // failures. https://crbug.com/1466313
                if (HelperTestRunner.sTestFailed) {
                    LifetimeAssert.resetForTesting();
                } else {
                    LifetimeAssert.assertAllInstancesDestroyedForTesting();
                }
            }
        }
    }

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
    protected void afterClass() {
        super.afterClass();
        ResettersForTesting.onAfterClass();
    }

    @Override
    protected void beforeTest(Sandbox sandbox, FrameworkMethod method, Method bootstrappedMethod)
            throws Throwable {
        ResettersForTesting.setMethodMode();
        super.beforeTest(sandbox, method, bootstrappedMethod);
    }

    @Override
    protected boolean isIgnored(FrameworkMethod method) {
        if (super.isIgnored(method) || method.getAnnotation(DisabledTest.class) != null) {
            return true;
        }
        Class<?> testSuiteClass = method.getDeclaringClass();
        return testSuiteClass.getAnnotation(DisabledTest.class) != null;
    }

    @Override
    protected InstrumentationConfiguration createClassLoaderConfig(final FrameworkMethod method) {
        return new InstrumentationConfiguration.Builder(super.createClassLoaderConfig(method))
                .doNotAcquireClass(HelperTestRunner.class)
                .doNotAcquireClass(TimeoutTimer.class) // Requires access to non-fake SystemClock.
                .doNotAcquireClass(ResettersForTesting.class)
                .build();
    }
}
