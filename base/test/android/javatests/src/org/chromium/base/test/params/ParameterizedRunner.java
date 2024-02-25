// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Test;
import org.junit.runner.Runner;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.Suite;
import org.junit.runners.model.FrameworkField;
import org.junit.runners.model.Statement;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunnerDelegateFactory.ParameterizedRunnerDelegateInstantiationException;

import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * ParameterizedRunner generates a list of runners for each of class parameter set in a test class.
 *
 * ParameterizedRunner looks for {@code @ClassParameter} annotation in test class and
 * generates a list of ParameterizedRunnerDelegate runners for each ParameterSet.
 */
public final class ParameterizedRunner extends Suite {
    private final List<Runner> mRunners;

    /**
     * Create a ParameterizedRunner to run test class
     *
     * @param klass the Class of the test class, test class should be atomic
     *              (extends only Object)
     */
    public ParameterizedRunner(Class<?> klass) throws Throwable {
        super(klass, Collections.emptyList()); // pass in empty list of runners
        validate();
        mRunners = createRunners(getTestClass());
    }

    @Override
    protected List<Runner> getChildren() {
        return mRunners;
    }

    /**
     * ParentRunner calls collectInitializationErrors() to check for errors in Test class.
     * Parameterized tests are written in unconventional ways, therefore, this method is
     * overridden and validation is done seperately.
     */
    @Override
    protected void collectInitializationErrors(List<Throwable> errors) {
        // Do not call super collectInitializationErrors
    }

    private void validate() throws Throwable {
        validateNoNonStaticInnerClass();
        validateOnlyOneConstructor();
        validateInstanceMethods();
        validateOnlyOneClassParameterField();
        validateAtLeastOneParameterSetField();
    }

    private void validateNoNonStaticInnerClass() throws Exception {
        if (getTestClass().isANonStaticInnerClass()) {
            throw new Exception("The inner class " + getTestClass().getName() + " is not static.");
        }
    }

    private void validateOnlyOneConstructor() throws Exception {
        if (!hasOneConstructor()) {
            throw new Exception("Test class should have exactly one public constructor");
        }
    }

    private boolean hasOneConstructor() {
        return getTestClass().getJavaClass().getConstructors().length == 1;
    }

    private void validateOnlyOneClassParameterField() {
        if (getTestClass().getAnnotatedFields(ClassParameter.class).size() > 1) {
            throw new IllegalParameterArgumentException(
                    String.format(
                            Locale.getDefault(),
                            "%s class has more than one @ClassParameter, only one is allowed",
                            getTestClass().getName()));
        }
    }

    private void validateAtLeastOneParameterSetField() {
        if (getTestClass().getAnnotatedFields(ClassParameter.class).isEmpty()
                && getTestClass().getAnnotatedMethods(UseMethodParameter.class).isEmpty()) {
            throw new IllegalArgumentException(
                    String.format(
                            Locale.getDefault(),
                            "%s has no field annotated with @ClassParameter or method annotated"
                                + " with@UseMethodParameter; it should not use ParameterizedRunner",
                            getTestClass().getName()));
        }
    }

    private void validateInstanceMethods() throws Exception {
        if (getTestClass().getAnnotatedMethods(Test.class).size() == 0) {
            throw new Exception("No runnable methods");
        }
    }

    /**
     * Return a list of runner delegates through ParameterizedRunnerDelegateFactory.
     *
     * For class parameter set: each class can only have one list of class parameter sets.
     * Each parameter set will be used to create one runner.
     *
     * For method parameter set: a single list method parameter sets is associated with
     * a string tag, an immutable map of string to parameter set list will be created and
     * passed into factory for each runner delegate to create multiple tests. Only one
     * Runner will be created for a method that uses @UseMethodParameter, regardless of the
     * number of ParameterSets in the associated list.
     *
     * @return a list of runners
     * @throws ParameterizedRunnerDelegateInstantiationException if runner delegate can not
     *         be instantiated with constructor reflectively
     * @throws IllegalAccessError if the field in tests are not accessible
     */
    static List<Runner> createRunners(TestClass testClass)
            throws IllegalAccessException, ParameterizedRunnerDelegateInstantiationException {
        List<ParameterSet> classParameterSetList;
        if (testClass.getAnnotatedFields(ClassParameter.class).isEmpty()) {
            classParameterSetList = new ArrayList<>();
            classParameterSetList.add(null);
        } else {
            classParameterSetList =
                    getParameterSetList(
                            testClass.getAnnotatedFields(ClassParameter.class).get(0), testClass);
            validateWidth(classParameterSetList);
        }

        Class<? extends ParameterizedRunnerDelegate> runnerDelegateClass =
                getRunnerDelegateClass(testClass);
        ParameterizedRunnerDelegateFactory factory = new ParameterizedRunnerDelegateFactory();
        List<Runner> runnersForTestClass = new ArrayList<>();
        for (ParameterSet classParameterSet : classParameterSetList) {
            BlockJUnit4ClassRunner runner =
                    (BlockJUnit4ClassRunner)
                            factory.createRunner(testClass, classParameterSet, runnerDelegateClass);
            runnersForTestClass.add(runner);
        }
        return runnersForTestClass;
    }

    /** Return an unmodifiable list of ParameterSet through a FrameworkField */
    private static List<ParameterSet> getParameterSetList(FrameworkField field, TestClass testClass)
            throws IllegalAccessException {
        field.getField().setAccessible(true);
        if (!Modifier.isStatic(field.getField().getModifiers())) {
            throw new IllegalParameterArgumentException(
                    String.format(
                            Locale.getDefault(),
                            "ParameterSetList fields must be static, this field %s in %s is not",
                            field.getName(),
                            testClass.getName()));
        }
        if (!(field.get(testClass.getJavaClass()) instanceof List)) {
            throw new IllegalArgumentException(
                    String.format(
                            Locale.getDefault(),
                            "Fields with @ClassParameter annotations must be an instance of List, "
                                    + "this field %s in %s is not list",
                            field.getName(),
                            testClass.getName()));
        }
        @SuppressWarnings("unchecked") // checked above
        List<ParameterSet> result = (List<ParameterSet>) field.get(testClass.getJavaClass());
        return Collections.unmodifiableList(result);
    }

    static void validateWidth(Iterable<ParameterSet> parameterSetList) {
        int lastSize = -1;
        for (ParameterSet set : parameterSetList) {
            if (set.size() == 0) {
                throw new IllegalParameterArgumentException(
                        "No parameter is added to method ParameterSet");
            }
            if (lastSize == -1 || set.size() == lastSize) {
                lastSize = set.size();
            } else {
                throw new IllegalParameterArgumentException(
                        String.format(
                                Locale.getDefault(),
                                "All ParameterSets in a list of ParameterSet must have equal"
                                        + " length. The current ParameterSet (%s) contains %d"
                                        + " parameters, while previous ParameterSet contains %d"
                                        + " parameters",
                                Arrays.toString(set.getValues().toArray()),
                                set.size(),
                                lastSize));
            }
        }
    }

    /**
     * Get the runner delegate class for the test class if {@code @UseRunnerDelegate} is used.
     * The default runner delegate is BaseJUnit4RunnerDelegate.class
     */
    private static Class<? extends ParameterizedRunnerDelegate> getRunnerDelegateClass(
            TestClass testClass) {
        if (testClass.getAnnotation(UseRunnerDelegate.class) != null) {
            return testClass.getAnnotation(UseRunnerDelegate.class).value();
        }
        return BaseJUnit4RunnerDelegate.class;
    }

    static class IllegalParameterArgumentException extends IllegalArgumentException {
        IllegalParameterArgumentException(String msg) {
            super(msg);
        }
    }

    public static class ParameterizedTestInstantiationException extends Exception {
        ParameterizedTestInstantiationException(
                TestClass testClass, String parameterSetString, Exception e) {
            super(
                    String.format(
                            "Test class %s can not be initiated, the provided parameters are %s,"
                                    + " the required parameter types are %s",
                            testClass.getJavaClass().toString(),
                            parameterSetString,
                            Arrays.toString(testClass.getOnlyConstructor().getParameterTypes())),
                    e);
        }
    }

    /**
     * We need to prevent the ParentRunner from running ClassRules or Before/AfterClass annotations,
     * or they'll run a second time (re-entrantly) when the child runners that actually run the
     * tests run.
     *
     * Do not call super.classBlock().
     */
    @Override
    protected Statement classBlock(final RunNotifier notifier) {
        Statement statement = childrenInvoker(notifier);
        return statement;
    }
}
