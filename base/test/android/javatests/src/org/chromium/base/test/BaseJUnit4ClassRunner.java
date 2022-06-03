// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import android.support.test.internal.util.AndroidRunnerParams;

import androidx.annotation.CallSuper;

import org.junit.rules.MethodRule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.base.test.util.MinAndroidSdkLevelSkipCheck;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.base.test.util.SkipCheck;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 *  A custom runner for JUnit4 tests that checks requirements to conditionally ignore tests.
 *
 *  This ClassRunner imports from AndroidJUnit4ClassRunner which is a hidden but accessible
 *  class. The reason is that default JUnit4 runner for Android is a final class,
 *  AndroidJUnit4. We need to extends an inheritable class to change {@link #runChild}
 *  and {@link #isIgnored} to add SkipChecks and PreTesthook.
 */
public class BaseJUnit4ClassRunner extends AndroidJUnit4ClassRunner {
    private static final String TAG = "BaseJUnit4ClassRunnr";

    private static final String EXTRA_TRACE_FILE =
            "org.chromium.base.test.BaseJUnit4ClassRunner.TraceFile";

    // Arbirary int that must not overlap with status codes defined by
    // https://developer.android.com/reference/android/test/InstrumentationTestRunner.html#REPORT_VALUE_ID
    private static final int STATUS_CODE_TEST_DURATION = 1337;
    private static final String DURATION_BUNDLE_ID = "duration_ms";

    /**
     * An interface for classes that have some code to run before (or after) the class is
     * instantiated. They run after {@Link BeforeClass} (or before @AfterClass) methods are called.
     * Provides access to the test class (and the annotations defined for it) and the
     * instrumentation context.
     *
     * The only reason to use a ClassHook instead of a TestRule is because @BeforeClass/@AfterClass
     * run during test listing, or multiple times for parameterized tests. See
     * https://crbug.com/1090043.
     *
     * TODO(https://crbug.com/1092646): Migrate all Class/Test Hooks to TestRules.
     */
    public interface ClassHook {
        /**
         * @param targetContext the instrumentation context that will be used during the test.
         * @param testMethod the test method to be run.
         */
        public void run(Context targetContext, Class<?> testClass);
    }

    /**
     * An interface for classes that have some code to run before a test. They run after
     * {@link SkipCheck}s and before {@Link Before} (or after @After). Provides access to the test
     * method (and the annotations defined for it) and the instrumentation context.
     *
     * Do not use TestHooks unless you also require ClassHooks. Otherwise, you should use TestRules
     * and {@link #getDefaultTestRules}.
     */
    public interface TestHook {
        /**
         * @param targetContext the instrumentation context that will be used during the test.
         * @param testMethod the test method to be run.
         */
        public void run(Context targetContext, FrameworkMethod testMethod);
    }

    /**
     * Create a BaseJUnit4ClassRunner to run {@code klass} and initialize values.
     *
     * @throws InitializationError if the test class malformed
     */
    public BaseJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass,
                new AndroidRunnerParams(InstrumentationRegistry.getInstrumentation(),
                        InstrumentationRegistry.getArguments(), false, 0L, false));

        assert InstrumentationRegistry.getInstrumentation()
                        instanceof BaseChromiumAndroidJUnitRunner
            : "Must use BaseChromiumAndroidJUnitRunner instrumentation with "
              + "BaseJUnit4ClassRunner, but found: "
              + InstrumentationRegistry.getInstrumentation().getClass();
        String traceOutput = InstrumentationRegistry.getArguments().getString(EXTRA_TRACE_FILE);

        if (traceOutput != null) {
            File traceOutputFile = new File(traceOutput);
            File traceOutputDir = traceOutputFile.getParentFile();

            if (traceOutputDir != null) {
                if (traceOutputDir.exists() || traceOutputDir.mkdirs()) {
                    TestTraceEvent.enable(traceOutputFile);
                }
            }
        }
    }

    /** Returns the singleton Application instance. */
    public static Application getApplication() {
        return (Application)
                BaseChromiumAndroidJUnitRunner.sInMemorySharedPreferencesContext.getBaseContext();
    }

    /**
     * Merge two List into a new ArrayList.
     *
     * Used to merge the default SkipChecks/PreTestHooks with the subclasses's
     * SkipChecks/PreTestHooks.
     */
    private static <T> List<T> mergeList(List<T> listA, List<T> listB) {
        List<T> l = new ArrayList<>(listA);
        l.addAll(listB);
        return l;
    }

    @SafeVarargs
    protected static <T> List<T> addToList(List<T> list, T... additionalEntries) {
        return mergeList(list, Arrays.asList(additionalEntries));
    }

    @Override
    protected void collectInitializationErrors(List<Throwable> errors) {
        super.collectInitializationErrors(errors);
        // Log any initialization errors to help debugging, as the host-side test runner can get
        // confused by the thrown exception.
        if (!errors.isEmpty()) {
            Log.e(TAG, "Initialization errors in %s: %s", getTestClass().getName(), errors);
        }
    }

    /**
     * Override this method to return a list of {@link SkipCheck}s}.
     *
     * Additional hooks can be added to the list using {@link #addToList}:
     * {@code return addToList(super.getSkipChecks(), check1, check2);}
     */
    @CallSuper
    protected List<SkipCheck> getSkipChecks() {
        return Arrays.asList(new RestrictionSkipCheck(InstrumentationRegistry.getTargetContext()),
                new MinAndroidSdkLevelSkipCheck(), new DisableIfSkipCheck());
    }

    /**
     * See {@link ClassHook}. Prefer to use TestRules over this.
     *
     * Additional hooks can be added to the list by overriding this method and using {@link
     * #addToList}:
     * {@code return addToList(super.getPreClassHooks(), hook1, hook2);}
     */
    @CallSuper
    protected List<ClassHook> getPreClassHooks() {
        return Arrays.asList(CommandLineFlags.getPreClassHook());
    }

    /**
     * See {@link ClassHook}. Prefer to use TestRules over this.
     *
     * Additional hooks can be added to the list by overriding this method and using {@link
     * #addToList}:
     * {@code return addToList(super.getPostClassHooks(), hook1, hook2);}
     */
    @CallSuper
    protected List<ClassHook> getPostClassHooks() {
        return Arrays.asList(CommandLineFlags.getPostClassHook());
    }

    /**
     * See {@link TestHook}. Prefer to use TestRules over this.
     *
     * Additional hooks can be added to the list by overriding this method and using {@link
     * #addToList}:
     * {@code return addToList(super.getPreTestHooks(), hook1, hook2);}
     */
    @CallSuper
    protected List<TestHook> getPreTestHooks() {
        return Arrays.asList(CommandLineFlags.getPreTestHook());
    }

    /**
     * See {@link TestHook}. Prefer to use TestRules over this.
     *
     * Additional hooks can be added to the list by overriding this method and using {@link
     * #addToList}:
     * {@code return addToList(super.getPostTestHooks(), hook1, hook2);}
     */
    @CallSuper
    protected List<TestHook> getPostTestHooks() {
        return Arrays.asList(CommandLineFlags.getPostTestHook());
    }

    /**
     * Override this method to return a list of method rules that should be applied to all tests
     * run with this test runner.
     *
     * Additional rules can be added to the list using {@link #addToList}:
     * {@code return addToList(super.getDefaultMethodRules(), rule1, rule2);}
     */
    @CallSuper
    protected List<MethodRule> getDefaultMethodRules() {
        return Collections.singletonList(new MethodParamAnnotationRule());
    }

    /**
     * Override this method to return a list of rules that should be applied to all tests run with
     * this test runner.
     *
     * Additional rules can be added to the list using {@link #addToList}:
     * {@code return addToList(super.getDefaultTestRules(), rule1, rule2);}
     */
    @CallSuper
    protected List<TestRule> getDefaultTestRules() {
        return Arrays.asList(new BaseJUnit4TestRule(), new MockitoErrorHandler());
    }

    /**
     * Evaluate whether a FrameworkMethod is ignored based on {@code SkipCheck}s.
     */
    @Override
    protected boolean isIgnored(FrameworkMethod method) {
        return super.isIgnored(method) || shouldSkip(method);
    }

    @Override
    protected List<MethodRule> rules(Object target) {
        List<MethodRule> declaredRules = super.rules(target);
        List<MethodRule> defaultRules = getDefaultMethodRules();
        return mergeList(defaultRules, declaredRules);
    }

    @Override
    protected final List<TestRule> getTestRules(Object target) {
        List<TestRule> declaredRules = super.getTestRules(target);
        List<TestRule> defaultRules = getDefaultTestRules();
        return mergeList(declaredRules, defaultRules);
    }

    /**
     * Run test with or without execution based on bundle arguments.
     */
    @Override
    public void run(RunNotifier notifier) {
        if (BaseChromiumAndroidJUnitRunner.shouldListTests()) {
            for (Description child : getDescription().getChildren()) {
                notifier.fireTestFinished(child);
            }
            return;
        }

        runPreClassHooks(getDescription().getTestClass());
        assert CommandLine.isInitialized();

        super.run(notifier);

        runPostClassHooks(getDescription().getTestClass());
    }

    @Override
    protected void runChild(FrameworkMethod method, RunNotifier notifier) {
        String testName = method.getName();
        TestTraceEvent.begin(testName);

        long start = SystemClock.uptimeMillis();

        runPreTestHooks(method);

        super.runChild(method, notifier);

        runPostTestHooks(method);

        Bundle b = new Bundle();
        b.putLong(DURATION_BUNDLE_ID, SystemClock.uptimeMillis() - start);
        InstrumentationRegistry.getInstrumentation().sendStatus(STATUS_CODE_TEST_DURATION, b);

        TestTraceEvent.end(testName);

        // A new instance of BaseJUnit4ClassRunner is created on the device
        // for each new method, so runChild will only be called once. Thus, we
        // can disable tracing, and dump the output, once we get here.
        TestTraceEvent.disable();
    }

    /**
     * Loop through all the {@code PreTestHook}s to run them
     */
    private void runPreTestHooks(FrameworkMethod frameworkMethod) {
        Context targetContext = InstrumentationRegistry.getTargetContext();
        for (TestHook hook : getPreTestHooks()) {
            hook.run(targetContext, frameworkMethod);
        }
    }

    private void runPreClassHooks(Class<?> klass) {
        Context targetContext = InstrumentationRegistry.getTargetContext();
        for (ClassHook hook : getPreClassHooks()) {
            hook.run(targetContext, klass);
        }
    }

    private void runPostTestHooks(FrameworkMethod frameworkMethod) {
        Context targetContext = InstrumentationRegistry.getTargetContext();
        for (TestHook hook : getPostTestHooks()) {
            hook.run(targetContext, frameworkMethod);
        }
    }

    private void runPostClassHooks(Class<?> klass) {
        Context targetContext = InstrumentationRegistry.getTargetContext();
        for (ClassHook hook : getPostClassHooks()) {
            hook.run(targetContext, klass);
        }
    }

    /**
     * Loop through all the {@code SkipCheck}s to confirm whether a test should be ignored
     */
    private boolean shouldSkip(FrameworkMethod method) {
        for (SkipCheck s : getSkipChecks()) {
            if (s.shouldSkip(method)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Overriding this method to take screenshot of failure before tear down functions are run.
     */
    @Override
    protected Statement withAfters(FrameworkMethod method, Object test, Statement base) {
        return super.withAfters(method, test, new ScreenshotOnFailureStatement(base));
    }

    /**
     * This function replicates the androidx AndroidJUnit4ClassRunner version of this function.
     * We can delete this override when we migrate to androidx.
     */
    @Override
    protected Statement methodInvoker(FrameworkMethod method, Object test) {
        if (UiThreadStatement.shouldRunOnUiThread(method)) {
            return new UiThreadStatement(super.methodInvoker(method, test));
        }
        return super.methodInvoker(method, test);
    }
}
