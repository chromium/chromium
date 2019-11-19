// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.hamcrest.Matchers.isIn;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.rules.ExternalResource;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.InitializationError;
import org.robolectric.RuntimeEnvironment;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.List;

/**
 * Helper rule to allow executing test runners in tests.
 *
 * Quis probat ipsas probas?
 */
class TestRunnerTestRule extends ExternalResource {
    final Class<? extends BlockJUnit4ClassRunner> mRunnerClass;

    /**
     * @param runnerClass The runner class to run the test
     */
    TestRunnerTestRule(Class<? extends BlockJUnit4ClassRunner> runnerClass) {
        mRunnerClass = runnerClass;
    }

    @Override
    protected void before() {
        // Register a fake Instrumentation so that class runners for instrumentation tests
        // can be run even in Robolectric tests.
        Instrumentation instrumentation = new Instrumentation() {
            @Override
            public Context getTargetContext() {
                return RuntimeEnvironment.application;
            }
        };
        InstrumentationRegistry.registerInstance(instrumentation, new Bundle());
    }

    @Override
    protected void after() {
        InstrumentationRegistry.registerInstance(null, new Bundle());
    }

    /**
     * A struct-like class containing lists of run and skipped tests.
     */
    public static class TestLog {
        public final List<Description> runTests = new ArrayList<>();
        public final List<Description> skippedTests = new ArrayList<>();
    }

    /**
     * Creates a new test runner and executes the test in the given {@code testClass} on it,
     * returning lists of tests that were run and tests that were skipped.
     * @param testClass The test class
     * @return A {@link TestLog} that contains lists of run and skipped tests.
     */
    public TestLog runTest(Class<?> testClass) throws InvocationTargetException,
                                                      NoSuchMethodException, InstantiationException,
                                                      IllegalAccessException {
        TestLog testLog = new TestLog();

        // TODO(bauerb): Using Mockito mock() or spy() throws a ClassCastException.
        RunListener runListener = new RunListener() {
            @Override
            public void testStarted(Description description) {
                testLog.runTests.add(description);
            }

            @Override
            public void testFinished(Description description) {
                Assert.assertThat(description, isIn(testLog.runTests));
            }

            @Override
            public void testFailure(Failure failure) {
                fail(failure.toString());
            }

            @Override
            public void testAssumptionFailure(Failure failure) {
                fail(failure.toString());
            }

            @Override
            public void testIgnored(Description description) {
                testLog.skippedTests.add(description);
            }
        };
        RunNotifier runNotifier = new RunNotifier();
        runNotifier.addListener(runListener);
        Runner runner;
        try {
            runner = mRunnerClass.getConstructor(Class.class).newInstance(testClass);
        } catch (InvocationTargetException e) {
            // If constructing the runner caused initialization errors, unwrap them from the
            // InvocationTargetException.
            Throwable cause = e.getCause();
            if (!(cause instanceof InitializationError)) throw e;
            List<Throwable> causes = ((InitializationError) cause).getCauses();

            // If there was exactly one initialization error, rewrap that one.
            if (causes.size() == 1) {
                throw new InvocationTargetException(causes.get(0), "Initialization error");
            }

            // Otherwise, serialize all initialization errors to a string and throw that.
            throw new AssertionError(causes.toString());
        }
        runner.run(runNotifier);
        return testLog;
    }
}
