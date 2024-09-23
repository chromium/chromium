// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.PathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner.HelperTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.build.NativeLibraries;

import java.lang.reflect.Method;
import java.util.Locale;
import java.util.TimeZone;

/**
 * The default Rule used by BaseRobolectricTestRunner. Include this directly when using
 * ParameterizedRobolectricTestRunner.
 * Use @Rule(order=-2) to ensure it runs before other rules.
 */
public class BaseRobolectricTestRule implements TestRule {
    private static final Locale ORIG_LOCALE = Locale.getDefault();
    private static final TimeZone ORIG_TIMEZONE = TimeZone.getDefault();

    // Removes the API Level suffix. E.g. "testSomething[28]" -> "testSomething".
    private static String stripBrackets(String methodName) {
        int idx = methodName.indexOf('[');
        if (idx != -1) {
            methodName = methodName.substring(0, idx);
        }
        return methodName;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp(
                        description
                                .getTestClass()
                                .getMethod(stripBrackets(description.getMethodName())));
                boolean testFailed = true;
                try {
                    base.evaluate();
                    testFailed = false;
                } finally {
                    tearDown(testFailed);
                }
            }
        };
    }

    static void setUp(Method method) {
        // Some of this logic seems like it would be more appropriate in @BeforeClass, but
        // Robolectric doesn't really support @BeforeClass (maybe because @Config can be applied to
        // individual methods). It does run the annotated methods, but does does so before
        // configuring the Application instance, and it does so from within methodBlock rather than
        // classBlock().
        ResettersForTesting.beforeHooksWillExecute();
        FeatureList.setDisableNativeForTesting(true);
        CommandLineFlags.ensureInitialized();
        UmaRecorderHolder.setUpNativeUmaRecorder(false);
        UmaRecorderHolder.resetForTesting();
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        ApplicationStatus.initialize(ApplicationProvider.getApplicationContext());

        Class<?> testClass = method.getDeclaringClass();
        CommandLineFlags.reset(testClass.getAnnotations(), method.getAnnotations());

        BundleUtils.resetForTesting();
        // Whether or not native is loaded is a global one-way switch, so do it automatically so
        // that it is always in the same state.
        if (NativeLibraries.LIBRARIES.length > 0) {
            LibraryLoader.getInstance().ensureMainDexInitialized();
        }
    }

    static void tearDown(boolean testFailed) {
        try {
            // https://crbug.com/1392817 for context as to why we do this.
            PostTask.flushJobsAndResetForTesting();
        } catch (InterruptedException e) {
            HelperTestRunner.sTestFailed = true;
            throw new RuntimeException(e);
        } finally {
            ApplicationStatus.destroyForJUnitTests();
            PathUtils.resetForTesting();
            ThreadUtils.clearUiThreadForTesting();
            Locale.setDefault(ORIG_LOCALE);
            TimeZone.setDefault(ORIG_TIMEZONE);
            ResettersForTesting.afterHooksDidExecute();
            // Run assertions only when the test has not already failed so as to not mask
            // failures. https://crbug.com/1466313
            if (testFailed) {
                LifetimeAssert.resetForTesting();
            } else {
                LifetimeAssert.assertAllInstancesDestroyedForTesting();
            }
        }
    }
}
