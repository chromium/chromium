// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterAfter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;

import java.util.Arrays;
import java.util.List;

/**
 * Example test that uses ParameterizedRunner
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public class ExampleParameterizedTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value("hello", "world").name("HelloWorld"),
                    new ParameterSet().value("Xxxx", "Yyyy").name("XxxxYyyy"),
                    new ParameterSet().value("aa", "yy").name("AaYy"));

    public static class MethodParamsA implements ParameterProvider {
        private static List<ParameterSet> sMethodParamA =
                Arrays.asList(new ParameterSet().value(1, 2).name("OneTwo"),
                        new ParameterSet().value(2, 3).name("TwoThree"),
                        new ParameterSet().value(3, 4).name("ThreeFour"));

        @Override
        public List<ParameterSet> getParameters() {
            return sMethodParamA;
        }
    }

    public static class MethodParamsB implements ParameterProvider {
        private static List<ParameterSet> sMethodParamB =
                Arrays.asList(new ParameterSet().value("a", "b").name("Ab"),
                        new ParameterSet().value("b", "c").name("Bc"),
                        new ParameterSet().value("c", "d").name("Cd"),
                        new ParameterSet().value("d", "e").name("De"));

        @Override
        public List<ParameterSet> getParameters() {
            return sMethodParamB;
        }
    }

    private String mStringA;
    private String mStringB;

    public ExampleParameterizedTest(String a, String b) {
        mStringA = a;
        mStringB = b;
    }

    @Test
    public void testSimple() {
        Assert.assertEquals(
                "A and B string length aren't equal", mStringA.length(), mStringB.length());
    }

    @Rule
    public MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private Integer mSum;

    @UseMethodParameterBefore(MethodParamsA.class)
    public void setupWithOnlyA(int intA, int intB) {
        mSum = intA + intB;
    }

    @Test
    @UseMethodParameter(MethodParamsA.class)
    public void testWithOnlyA(int intA, int intB) {
        Assert.assertEquals(intA + 1, intB);
        Assert.assertEquals(mSum, Integer.valueOf(intA + intB));
        mSum = null;
    }

    private String mConcatenation;

    @Test
    @UseMethodParameter(MethodParamsB.class)
    public void testWithOnlyB(String a, String b) {
        Assert.assertTrue(!a.equals(b));
        mConcatenation = a + b;
    }

    @UseMethodParameterAfter(MethodParamsB.class)
    public void teardownWithOnlyB(String a, String b) {
        Assert.assertEquals(mConcatenation, a + b);
        mConcatenation = null;
    }
}
