// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runner.Runner;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

/**
 * Test for verify the names and test method Description works properly
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ParameterizedTestNameTest {
    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class TestClassWithClassParameterAppendName {
        @ClassParameter
        static List<ParameterSet> sAllName = Arrays.asList(
                new ParameterSet().value("hello").name("Hello"),
                new ParameterSet().value("world").name("World")
        );

        public TestClassWithClassParameterAppendName(String a) {}

        @Test
        public void test() {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class TestClassWithClassParameterDefaultName {
        @ClassParameter
        static List<ParameterSet> sAllName = Arrays.asList(
                new ParameterSet().value("hello"),
                new ParameterSet().value("world")
        );

        public TestClassWithClassParameterDefaultName(String a) {}

        @Test
        public void test() {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class TestClassWithMethodParameter {
        static class AppendNameParams implements ParameterProvider {
            @Override
            public Iterable<ParameterSet> getParameters() {
                return Arrays.asList(
                        new ParameterSet().value("hello").name("Hello"),
                        new ParameterSet().value("world").name("World")
                );
            }
        }

        static class DefaultNameParams implements ParameterProvider {
            @Override
            public Iterable<ParameterSet> getParameters() {
                return Arrays.asList(
                        new ParameterSet().value("hello"),
                        new ParameterSet().value("world")
                );
            }
        }

        @UseMethodParameter(AppendNameParams.class)
        @Test
        public void test(String a) {}

        @UseMethodParameter(DefaultNameParams.class)
        @Test
        public void testDefaultName(String b) {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class TestClassWithMixedParameter {
        @ClassParameter
        static List<ParameterSet> sAllName = Arrays.asList(
                new ParameterSet().value("hello").name("Hello"),
                new ParameterSet().value("world").name("World")
        );

        static class AppendNameParams implements ParameterProvider {
            @Override
            public Iterable<ParameterSet> getParameters() {
                return Arrays.asList(
                        new ParameterSet().value("1").name("A"),
                        new ParameterSet().value("2").name("B")
                );
            }
        }

        public TestClassWithMixedParameter(String a) {}

        @UseMethodParameter(AppendNameParams.class)
        @Test
        public void testA(String a) {}

        @Test
        public void test() {}
    }

    @Test
    public void testClassParameterAppendName() throws Throwable {
        List<Runner> runners = ParameterizedRunner.createRunners(
                new TestClass(TestClassWithClassParameterAppendName.class));
        List<String> expectedTestNames =
                new LinkedList<String>(Arrays.asList("test__Hello", "test__World"));
        List<String> computedMethodNames = new ArrayList<>();
        for (Runner r : runners) {
            BlockJUnit4RunnerDelegate castedRunner = (BlockJUnit4RunnerDelegate) r;
            for (FrameworkMethod method : castedRunner.computeTestMethods()) {
                computedMethodNames.add(method.getName());
                Assert.assertTrue("This test name is not expected: " + method.getName(),
                        expectedTestNames.contains(method.getName()));
                expectedTestNames.remove(method.getName());
                method.getName();
            }
        }
        Assert.assertTrue(
                String.format(
                        "These names were provided: %s, these expected names are not found: %s",
                        Arrays.toString(computedMethodNames.toArray()),
                        Arrays.toString(expectedTestNames.toArray())),
                expectedTestNames.isEmpty());
    }

    @Test
    public void testClassParameterDefaultName() throws Throwable {
        List<Runner> runners = ParameterizedRunner.createRunners(
                new TestClass(TestClassWithClassParameterDefaultName.class));
        List<String> expectedTestNames = new LinkedList<String>(Arrays.asList("test", "test"));
        for (Runner r : runners) {
            @SuppressWarnings("unchecked")
            BlockJUnit4RunnerDelegate castedRunner = (BlockJUnit4RunnerDelegate) r;
            for (FrameworkMethod method : castedRunner.computeTestMethods()) {
                Assert.assertTrue("This test name is not expected: " + method.getName(),
                        expectedTestNames.contains(method.getName()));
                expectedTestNames.remove(method.getName());
                method.getName();
            }
        }
        Assert.assertTrue("These expected names are not found: "
                        + Arrays.toString(expectedTestNames.toArray()),
                expectedTestNames.isEmpty());
    }

    @Test
    public void testMethodParameter() throws Throwable {
        List<Runner> runners = ParameterizedRunner.createRunners(
                new TestClass(TestClassWithMethodParameter.class));
        List<String> expectedTestNames = new LinkedList<String>(
                Arrays.asList("test__Hello", "test__World", "testDefaultName", "testDefaultName"));
        for (Runner r : runners) {
            BlockJUnit4RunnerDelegate castedRunner = (BlockJUnit4RunnerDelegate) r;
            for (FrameworkMethod method : castedRunner.computeTestMethods()) {
                Assert.assertTrue("This test name is not expected: " + method.getName(),
                        expectedTestNames.contains(method.getName()));
                expectedTestNames.remove(method.getName());
                method.getName();
            }
        }
        Assert.assertTrue("These expected names are not found: "
                        + Arrays.toString(expectedTestNames.toArray()),
                expectedTestNames.isEmpty());
    }

    @Test
    public void testMixedParameterTestA() throws Throwable {
        List<Runner> runners =
                ParameterizedRunner.createRunners(new TestClass(TestClassWithMixedParameter.class));
        List<String> expectedTestNames =
                new LinkedList<String>(Arrays.asList("testA__Hello_A", "testA__World_A",
                        "testA__Hello_B", "testA__World_B", "test__Hello", "test__World"));
        for (Runner r : runners) {
            BlockJUnit4RunnerDelegate castedRunner = (BlockJUnit4RunnerDelegate) r;
            for (FrameworkMethod method : castedRunner.computeTestMethods()) {
                Assert.assertTrue("This test name is not expected: " + method.getName(),
                        expectedTestNames.contains(method.getName()));
                expectedTestNames.remove(method.getName());
                method.getName();
            }
        }
        Assert.assertTrue("These expected names are not found: "
                        + Arrays.toString(expectedTestNames.toArray()),
                expectedTestNames.isEmpty());
    }
}
