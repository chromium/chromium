// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner.IllegalParameterArgumentException;

import java.util.ArrayList;
import java.util.List;

/**
 * Test for org.chromium.base.test.params.ParameterizedRunner
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ParameterizedRunnerTest {
    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class BadTestClassWithMoreThanOneConstructor {
        @ClassParameter
        static List<ParameterSet> sClassParams = new ArrayList<>();

        public BadTestClassWithMoreThanOneConstructor() {}

        public BadTestClassWithMoreThanOneConstructor(String x) {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class BadTestClassWithNonListParameters {
        @ClassParameter
        static String[] sMethodParamA = {"1", "2"};

        @Test
        public void test() {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class BadTestClassWithoutNeedForParameterization {
        @Test
        public void test() {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class BadTestClassWithNonStaticParameterSetList {
        @ClassParameter
        public List<ParameterSet> mClassParams = new ArrayList<>();

        @Test
        public void test() {}
    }

    @UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
    public static class BadTestClassWithMultipleClassParameter {
        @ClassParameter
        private static List<ParameterSet> sParamA = new ArrayList<>();

        @ClassParameter
        private static List<ParameterSet> sParamB = new ArrayList<>();
    }

    @Test(expected = ParameterizedRunner.IllegalParameterArgumentException.class)
    public void testEmptyParameterSet() {
        List<ParameterSet> paramList = new ArrayList<>();
        paramList.add(new ParameterSet());
        ParameterizedRunner.validateWidth(paramList);
    }

    @Test(expected = ParameterizedRunner.IllegalParameterArgumentException.class)
    public void testUnequalWidthParameterSetList() {
        List<ParameterSet> paramList = new ArrayList<>();
        paramList.add(new ParameterSet().value(1, 2));
        paramList.add(new ParameterSet().value(3, 4, 5));
        ParameterizedRunner.validateWidth(paramList);
    }

    @Test(expected = ParameterizedRunner.IllegalParameterArgumentException.class)
    public void testUnequalWidthParameterSetListWithNull() {
        List<ParameterSet> paramList = new ArrayList<>();
        paramList.add(new ParameterSet().value(null));
        paramList.add(new ParameterSet().value(1, 2));
        ParameterizedRunner.validateWidth(paramList);
    }

    // This test ensures the class ParameterSet throws IllegalArgumentException
    // when passed an unacceptable data type.
    @Test(expected = IllegalArgumentException.class)
    @SuppressWarnings("ModifiedButNotUsed")
    public void testUnsupportedParameterType() throws Throwable {
        class MyPair {};
        List<ParameterSet> paramList = new ArrayList<>();
        paramList.add(new ParameterSet().value(new MyPair()));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBadClassWithNonListParameters() throws Throwable {
        new ParameterizedRunner(BadTestClassWithNonListParameters.class);
    }

    @Test(expected = IllegalParameterArgumentException.class)
    public void testBadClassWithNonStaticParameterSetList() throws Throwable {
        new ParameterizedRunner(BadTestClassWithNonStaticParameterSetList.class);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBadClassWithoutNeedForParameterization() throws Throwable {
        new ParameterizedRunner(BadTestClassWithoutNeedForParameterization.class);
    }

    @Test(expected = Exception.class)
    public void testBadClassWithMoreThanOneConstructor() throws Throwable {
        new ParameterizedRunner(BadTestClassWithMoreThanOneConstructor.class);
    }
}
