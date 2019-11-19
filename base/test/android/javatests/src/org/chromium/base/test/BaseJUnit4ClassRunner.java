// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Application;
import android.content.Context;
import android.support.annotation.CallSuper;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import android.support.test.internal.util.AndroidRunnerParams;

import org.junit.rules.MethodRule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.base.test.util.MinAndroidSdkLevelSkipCheck;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.base.test.util.SkipCheck;

import java.io.File;
import java.lang.reflect.Method;
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

    /**
     * Create a BaseJUnit4ClassRunner to run {@code klass} and initialize values.
     *
     * To add more SkipCheck or PreTestHook in subclass, create Lists of checks and hooks,
     * pass them into the super constructors. If you want make a subclass extendable by other
     * class runners, you also have to create a constructor similar to the following one that
     * merges default checks or hooks with this checks and hooks passed in by constructor.
     *
     * <pre>
     * <code>
     * e.g.
     * public ChildRunner extends BaseJUnit4ClassRunner {
     *     public ChildRunner(final Class<?> klass) {
     *             throws InitializationError {
     *         this(klass, Collections.emptyList(), Collections.emptyList(),
     * Collections.emptyList());
     *     }
     *
     *     public ChildRunner(
     *             final Class<?> klass, List<SkipCheck> checks, List<PreTestHook> hook,
     * List<TestRule> rules) { throws InitializationError { super(klass, mergeList( checks,
     * getSkipChecks()), mergeList(hooks, getPreTestHooks()));
     *     }
     *
     *     public List<SkipCheck> getSkipChecks() {...}
     *
     *     public List<PreTestHook> getPreTestHooks() {...}
     * </code>
     * </pre>
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
     * Override this method to return a list of {@link PreTestHook}s.
     *
     * Additional hooks can be added to the list using {@link #addToList}:
     * {@code return addToList(super.getPreTestHooks(), hook1, hook2);}
     * TODO(bauerb): Migrate PreTestHook to TestRule.
     */
    @CallSuper
    protected List<PreTestHook> getPreTestHooks() {
        return Collections.emptyList();
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
        // Order is important here. Outer rule setUp's run first, and tearDown's run last.
        // Base setUp() should go first to initialize ContextUtils and clear out prefs.
        // Base's tearDown() should come last since it deletes files.
        // Activities must be destroyed before lifetimes are checked, so DestroyActivitiesRule()
        // must come last so that its tearDown() runs before LifetimeAssertRule's.
        return Collections.singletonList(RuleChain.outerRule(new BaseJUnit4TestRule())
                                                 .around(new LifetimeAssertRule())
                                                 .around(new DestroyActivitiesRule()));
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
        if (BaseChromiumAndroidJUnitRunner.shouldListTests(
                    InstrumentationRegistry.getArguments())) {
            for (Description child : getDescription().getChildren()) {
                notifier.fireTestFinished(child);
            }
            return;
        }

        if (!CommandLine.isInitialized()) {
            initCommandLineForTest();
        }
        super.run(notifier);
    }

    /**
     * Override this method to change how test class runner initiate commandline flags
     */
    protected void initCommandLineForTest() {
        CommandLine.init(null);
    }

    @Override
    protected void runChild(FrameworkMethod method, RunNotifier notifier) {
        String testName = method.getName();
        TestTraceEvent.begin(testName);

        runPreTestHooks(method);

        super.runChild(method, notifier);

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
        Method testMethod = frameworkMethod.getMethod();
        Context targetContext = InstrumentationRegistry.getTargetContext();
        for (PreTestHook hook : getPreTestHooks()) {
            hook.run(targetContext, testMethod);
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

    /*
     * Overriding this method to take screenshot of failure before tear down functions are run.
     */
    @Override
    protected Statement withAfters(FrameworkMethod method, Object test, Statement base) {
        return super.withAfters(method, test, new ScreenshotOnFailureStatement(base));
    }
}
