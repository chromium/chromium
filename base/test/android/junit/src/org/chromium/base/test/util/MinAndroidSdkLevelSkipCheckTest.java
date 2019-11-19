// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.isIn;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for MinAndroidSdkLevelSkipCheck. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 21)
public class MinAndroidSdkLevelSkipCheckTest {
    public static class UnannotatedBaseClass {
        @Test
        @MinAndroidSdkLevel(20)
        public void min20Method() {}
        @Test
        @MinAndroidSdkLevel(22)
        public void min22Method() {}
    }

    @MinAndroidSdkLevel(20)
    public static class Min20Class extends UnannotatedBaseClass {
        @Test public void unannotatedMethod() {}
    }

    @MinAndroidSdkLevel(22)
    public static class Min22Class extends UnannotatedBaseClass {
        @Test public void unannotatedMethod() {}
    }

    public static class ExtendsMin20Class extends Min20Class {
        @Override
        @Test public void unannotatedMethod() {}
    }

    public static class ExtendsMin22Class extends Min22Class {
        @Override
        @Test public void unannotatedMethod() {}
    }

    private static final MinAndroidSdkLevelSkipCheck sSkipCheck = new MinAndroidSdkLevelSkipCheck();

    private static class InnerTestRunner extends BlockJUnit4ClassRunner {
        public InnerTestRunner(Class<?> klass) throws InitializationError {
            super(klass);
        }

        @Override
        protected boolean isIgnored(FrameworkMethod method) {
            return super.isIgnored(method) || sSkipCheck.shouldSkip(method);
        }
    }

    @Rule
    public TestRunnerTestRule mTestRunnerTestRule = new TestRunnerTestRule(InnerTestRunner.class);

    private void expectShouldSkip(Class<?> testClass, String methodName, boolean shouldSkip)
            throws Exception {
        Assert.assertThat(
                sSkipCheck.shouldSkip(new FrameworkMethod(testClass.getMethod(methodName))),
                equalTo(shouldSkip));
        TestRunnerTestRule.TestLog runListener = mTestRunnerTestRule.runTest(testClass);
        Assert.assertThat(Description.createTestDescription(testClass, methodName),
                isIn(shouldSkip ? runListener.skippedTests : runListener.runTests));
    }

    @Test
    public void testAnnotatedMethodAboveMin() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min20Method", false);
    }

    @Test
    public void testAnnotatedMethodBelowMin() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min22Method", true);
    }

    @Test
    public void testAnnotatedClassAboveMin() throws Exception {
        expectShouldSkip(Min20Class.class, "unannotatedMethod", false);
    }

    @Test
    public void testAnnotatedClassBelowMin() throws Exception {
        expectShouldSkip(Min22Class.class, "unannotatedMethod", true);
    }

    @Test
    public void testAnnotatedSuperclassAboveMin() throws Exception {
        expectShouldSkip(ExtendsMin20Class.class, "unannotatedMethod", false);
    }

    @Test
    public void testAnnotatedSuperclassBelowMin() throws Exception {
        expectShouldSkip(ExtendsMin22Class.class, "unannotatedMethod", true);
    }
}
