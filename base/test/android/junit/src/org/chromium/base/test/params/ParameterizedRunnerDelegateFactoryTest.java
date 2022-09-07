// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterizedRunnerDelegateFactory.ParameterizedRunnerDelegateInstantiationException;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Test for org.chromium.base.test.params.ParameterizedRunnerDelegateFactory
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ParameterizedRunnerDelegateFactoryTest {
    /**
     * This RunnerDelegate calls `super.collectInitializationErrors()` and would
     * cause BlockJUnit4ClassRunner to validate test classes.
     */
    public static class BadExampleRunnerDelegate
            extends BlockJUnit4ClassRunner implements ParameterizedRunnerDelegate {
        public static class LalaTestClass {}

        private final List<FrameworkMethod> mParameterizedFrameworkMethodList;

        BadExampleRunnerDelegate(Class<?> klass,
                List<FrameworkMethod> parameterizedFrameworkMethods) throws InitializationError {
            super(klass);
            mParameterizedFrameworkMethodList = parameterizedFrameworkMethods;
        }

        @Override
        public void collectInitializationErrors(List<Throwable> errors) {
            super.collectInitializationErrors(errors); // This is wrong!!
        }

        @Override
        public List<FrameworkMethod> computeTestMethods() {
            return mParameterizedFrameworkMethodList;
        }

        @Override
        public Object createTest() {
            return null;
        }
    }

    static class ExampleTestClass {
        static class MethodParamsA implements ParameterProvider {
            @Override
            public Iterable<ParameterSet> getParameters() {
                return Arrays.asList(
                        new ParameterSet().value("a").name("testWithValue_a"),
                        new ParameterSet().value("b").name("testWithValue_b")
                );
            }
        }

        @SuppressWarnings("unused")
        @UseMethodParameter(MethodParamsA.class)
        @Test
        public void testA(String a) {}

        static class MethodParamsB implements ParameterProvider {
            @Override
            public Iterable<ParameterSet> getParameters() {
                return Arrays.asList(
                        new ParameterSet().value(1).name("testWithValue_1"),
                        new ParameterSet().value(2).name("testWithValue_2"),
                        new ParameterSet().value(3).name("testWithValue_3")
                );
            }
        }

        @SuppressWarnings("unused")
        @UseMethodParameter(MethodParamsB.class)
        @Test
        public void testB(int b) {}

        @Test
        public void testByMyself() {}
    }

    /**
     * This test validates ParameterizedRunnerDelegateFactory throws exception when
     * a runner delegate does not override the collectInitializationErrors method.
     */
    @Test(expected = ParameterizedRunnerDelegateInstantiationException.class)
    public void testBadRunnerDelegateWithIncorrectValidationCall() throws Throwable {
        ParameterizedRunnerDelegateFactory factory = new ParameterizedRunnerDelegateFactory();
        TestClass testClass = new TestClass(BadExampleRunnerDelegate.LalaTestClass.class);
        factory.createRunner(testClass, null, BadExampleRunnerDelegate.class);
    }

    @Test
    public void testGenerateParameterizedFrameworkMethod() throws Throwable {
        List<FrameworkMethod> methods =
                ParameterizedRunnerDelegateFactory.generateUnmodifiableFrameworkMethodList(
                        new TestClass(ExampleTestClass.class), "");

        Assert.assertEquals(methods.size(), 6);

        Map<String, Method> expectedTests = new HashMap<>();
        Method testMethodA = ExampleTestClass.class.getDeclaredMethod("testA", String.class);
        Method testMethodB = ExampleTestClass.class.getDeclaredMethod("testB", int.class);
        Method testMethodByMyself = ExampleTestClass.class.getDeclaredMethod("testByMyself");
        expectedTests.put("testA__testWithValue_a", testMethodA);
        expectedTests.put("testA__testWithValue_b", testMethodA);
        expectedTests.put("testB__testWithValue_1", testMethodB);
        expectedTests.put("testB__testWithValue_2", testMethodB);
        expectedTests.put("testB__testWithValue_3", testMethodB);
        expectedTests.put("testByMyself", testMethodByMyself);
        for (FrameworkMethod method : methods) {
            Assert.assertNotNull(expectedTests.get(method.getName()));
            Assert.assertEquals(expectedTests.get(method.getName()), method.getMethod());
            expectedTests.remove(method.getName());
        }
        Assert.assertTrue(expectedTests.isEmpty());
    }
}
