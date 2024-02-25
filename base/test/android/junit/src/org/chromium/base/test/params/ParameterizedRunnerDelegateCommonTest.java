// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterizedRunner.ParameterizedTestInstantiationException;

import java.util.Collections;

@RunWith(BlockJUnit4ClassRunner.class)
public class ParameterizedRunnerDelegateCommonTest {
    /**
     * Create a test object using the list of class parameter set
     *
     * @param testClass the {@link TestClass} object for current test class
     * @param classParameterSet the parameter set needed for the test class constructor
     */
    private static Object createTest(TestClass testClass, ParameterSet classParameterSet)
            throws ParameterizedTestInstantiationException {
        return new ParameterizedRunnerDelegateCommon(
                        testClass, classParameterSet, Collections.emptyList())
                .createTest();
    }

    static class BadTestClassWithMoreThanOneConstructor {
        public BadTestClassWithMoreThanOneConstructor() {}

        @SuppressWarnings("unused")
        public BadTestClassWithMoreThanOneConstructor(String argument) {}
    }

    static class BadTestClassWithTwoArgumentConstructor {
        @SuppressWarnings("unused")
        public BadTestClassWithTwoArgumentConstructor(int a, int b) {}
    }

    abstract static class BadTestClassAbstract {
        public BadTestClassAbstract() {}
    }

    static class BadTestClassConstructorThrows {
        public BadTestClassConstructorThrows() {
            throw new RuntimeException();
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreateTestWithMoreThanOneConstructor() throws Throwable {
        TestClass testClass = new TestClass(BadTestClassWithMoreThanOneConstructor.class);
        createTest(testClass, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreateTestWithIncorrectArguments() throws Throwable {
        TestClass testClass = new TestClass(BadTestClassWithTwoArgumentConstructor.class);
        ParameterSet pSet = new ParameterSet().value(1, 2, 3);
        createTest(testClass, pSet);
    }

    @Test(expected = ParameterizedTestInstantiationException.class)
    public void testCreateTestWithAbstractClass() throws ParameterizedTestInstantiationException {
        TestClass testClass = new TestClass(BadTestClassAbstract.class);
        createTest(testClass, null);
    }

    @Test(expected = ParameterizedTestInstantiationException.class)
    public void testCreateTestWithThrowingConstructor()
            throws ParameterizedTestInstantiationException {
        TestClass testClass = new TestClass(BadTestClassConstructorThrows.class);
        createTest(testClass, null);
    }
}
