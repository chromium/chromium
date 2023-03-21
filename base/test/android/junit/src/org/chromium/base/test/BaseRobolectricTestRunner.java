// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.robolectric.DefaultTestLifecycle;
import org.robolectric.TestLifecycle;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Flag;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.lang.reflect.Method;

/**
 * A Robolectric Test Runner that configures Chromium-specific settings and initializes base
 * globals. If initializing base globals is not desired, then {@link
 * org.chromium.testing.local.LocalRobolectricTestRunner} could be used.
 */
public class BaseRobolectricTestRunner extends LocalRobolectricTestRunner {
    /**
     * Enables a per-test setUp / tearDown hook.
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
                LifetimeAssert.assertAllInstancesDestroyedForTesting();
            } finally {
                try {
                    // https://crbug.com/1392817 for context as to why we do this.
                    PostTask.flushJobsAndResetForTesting();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                } finally {
                    CommandLineFlags.tearDownMethod();
                    CommandLineFlags.tearDownClass();
                    ApplicationStatus.destroyForJUnitTests();
                    ContextUtils.clearApplicationContextForTests();
                    PathUtils.resetForTesting();
                    ThreadUtils.setThreadAssertsDisabledForTesting(false);
                    ThreadUtils.clearUiThreadForTesting();
                    super.afterTest(method);
                }
            }
        }
    }

    public BaseRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    @Override
    protected Class<? extends TestLifecycle> getTestLifecycleClass() {
        return BaseTestLifecycle.class;
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
