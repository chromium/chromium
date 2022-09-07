// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Test;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Factory to generate delegate class runners for ParameterizedRunner
 */
public class ParameterizedRunnerDelegateFactory {
    /**
     * Create a runner that implements ParameterizedRunner and extends BlockJUnit4ClassRunner
     *
     * @param testClass the TestClass object for current test class
     * @param classParameterSet A parameter set for test constructor arguments
     * @param parameterizedRunnerDelegateClass the parameterized runner delegate class specified
     *                                         through {@code @UseRunnerDelegate}
     */
    <T extends ParameterizedRunnerDelegate> T createRunner(TestClass testClass,
            ParameterSet classParameterSet, Class<T> parameterizedRunnerDelegateClass)
            throws ParameterizedRunnerDelegateInstantiationException {
        String testMethodPostfix = classParameterSet == null ? null : classParameterSet.getName();
        List<FrameworkMethod> unmodifiableFrameworkMethodList =
                generateUnmodifiableFrameworkMethodList(testClass, testMethodPostfix);
        ParameterizedRunnerDelegateCommon delegateCommon = new ParameterizedRunnerDelegateCommon(
                testClass, classParameterSet, unmodifiableFrameworkMethodList);
        try {
            return parameterizedRunnerDelegateClass
                    .getDeclaredConstructor(Class.class, ParameterizedRunnerDelegateCommon.class)
                    .newInstance(testClass.getJavaClass(), delegateCommon);
        } catch (Exception e) {
            throw new ParameterizedRunnerDelegateInstantiationException(
                    parameterizedRunnerDelegateClass.toString(), e);
        }
    }

    /**
     * Match test methods annotated by @UseMethodParameter(X) with
     * ParameterSetList annotated by @MethodParameter(X)
     *
     * @param testClass a {@code TestClass} that wraps around the actual java
     *            test class
     * @param postFix a name postfix for each test
     * @return a list of ParameterizedFrameworkMethod
     */
    static List<FrameworkMethod> generateUnmodifiableFrameworkMethodList(
            TestClass testClass, String postFix) {
        // Represent the list of all ParameterizedFrameworkMethod in this test class
        List<FrameworkMethod> returnList = new ArrayList<>();

        for (FrameworkMethod method : testClass.getAnnotatedMethods(Test.class)) {
            if (method.getMethod().isAnnotationPresent(UseMethodParameter.class)) {
                Iterable<ParameterSet> parameterSets =
                        getParameters(method.getAnnotation(UseMethodParameter.class).value());
                returnList.addAll(createParameterizedMethods(method, parameterSets, postFix));
            } else {
                // If test method is not parameterized (does not have UseMethodParameter annotation)
                returnList.add(new ParameterizedFrameworkMethod(method.getMethod(), null, postFix));
            }
        }

        return Collections.unmodifiableList(returnList);
    }

    /**
     * Exception caused by instantiating the provided Runner delegate
     * Potentially caused by not overriding collecInitializationErrors() method
     * to be empty
     */
    public static class ParameterizedRunnerDelegateInstantiationException extends Exception {
        private ParameterizedRunnerDelegateInstantiationException(
                String runnerDelegateClass, Exception e) {
            super(String.format("Current class runner delegate %s can not be instantiated.",
                          runnerDelegateClass),
                    e);
        }
    }

    private static Iterable<ParameterSet> getParameters(Class<? extends ParameterProvider> clazz) {
        ParameterProvider parameterProvider;
        try {
            parameterProvider = clazz.getDeclaredConstructor().newInstance();
        } catch (IllegalAccessException e) {
            throw new IllegalStateException("Failed instantiating " + clazz.getCanonicalName(), e);
        } catch (InstantiationException e) {
            throw new IllegalStateException("Failed instantiating " + clazz.getCanonicalName(), e);
        } catch (NoSuchMethodException e) {
            throw new IllegalStateException("Failed instantiating " + clazz.getCanonicalName(), e);
        } catch (InvocationTargetException e) {
            throw new IllegalStateException("Failed instantiating " + clazz.getCanonicalName(), e);
        }
        return parameterProvider.getParameters();
    }

    private static List<FrameworkMethod> createParameterizedMethods(
            FrameworkMethod baseMethod, Iterable<ParameterSet> parameterSetList, String suffix) {
        ParameterizedRunner.validateWidth(parameterSetList);
        List<FrameworkMethod> returnList = new ArrayList<>();
        for (ParameterSet set : parameterSetList) {
            returnList.add(new ParameterizedFrameworkMethod(baseMethod.getMethod(), set, suffix));
        }
        return returnList;
    }
}
