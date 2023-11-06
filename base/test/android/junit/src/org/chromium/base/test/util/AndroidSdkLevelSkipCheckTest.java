// Copyright 2022 The Chromium Authors
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

/** Unit tests for {@link AndroidSdkLevelSkipCheck} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 29)
public class AndroidSdkLevelSkipCheckTest {
    public static class UnannotatedBaseClass {
        @Test
        @MinAndroidSdkLevel(28)
        public void min28Method() {}

        @Test
        @MinAndroidSdkLevel(29)
        public void min29Method() {}

        @Test
        @MinAndroidSdkLevel(30)
        public void min30Method() {}

        @Test
        @MaxAndroidSdkLevel(28)
        public void max28Method() {}

        @Test
        @MaxAndroidSdkLevel(29)
        public void max29Method() {}

        @Test
        @MaxAndroidSdkLevel(30)
        public void max30Method() {}

        @Test
        @MinAndroidSdkLevel(28)
        @MaxAndroidSdkLevel(30)
        public void min28max30Method() {}

        @Test
        @MinAndroidSdkLevel(30)
        @MaxAndroidSdkLevel(28)
        public void min30max28Method() {}
    }

    @MinAndroidSdkLevel(28)
    public static class Min28Class extends UnannotatedBaseClass {
        @Test
        public void unannotatedMethod() {}
    }

    @MinAndroidSdkLevel(30)
    public static class Min30Class extends UnannotatedBaseClass {
        @Test
        public void unannotatedMethod() {}
    }

    public static class ExtendsMin28Class extends Min28Class {
        @Override
        @Test
        public void unannotatedMethod() {}
    }

    public static class ExtendsMin30Class extends Min30Class {
        @Override
        @Test
        public void unannotatedMethod() {}
    }

    private static final AndroidSdkLevelSkipCheck sSkipCheck = new AndroidSdkLevelSkipCheck();

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
        Assert.assertThat(
                Description.createTestDescription(testClass, methodName),
                isIn(shouldSkip ? runListener.skippedTests : runListener.runTests));
    }

    // Test {@link MinAndroidSdkLevel}

    @Test
    public void testAnnotatedMethodAboveMin_run() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min28Method", false);
    }

    @Test
    public void testAnnotatedMethodAtMin_run() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min29Method", false);
    }

    @Test
    public void testAnnotatedMethodBelowMin_skip() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min30Method", true);
    }

    @Test
    public void testAnnotatedClassAboveMin_run() throws Exception {
        expectShouldSkip(Min28Class.class, "unannotatedMethod", false);
    }

    @Test
    public void testAnnotatedClassBelowMin_skip() throws Exception {
        expectShouldSkip(Min30Class.class, "unannotatedMethod", true);
    }

    @Test
    public void testAnnotatedSuperclassAboveMin_run() throws Exception {
        expectShouldSkip(ExtendsMin28Class.class, "unannotatedMethod", false);
    }

    @Test
    public void testAnnotatedSuperclassBelowMin_skip() throws Exception {
        expectShouldSkip(ExtendsMin30Class.class, "unannotatedMethod", true);
    }

    // Test {@link MaxAndroidSdkLevel}

    @Test
    public void testAnnotatedMethodAboveMax_skip() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "max28Method", true);
    }

    @Test
    public void testAnnotatedMethodAtMax_run() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "max29Method", false);
    }

    @Test
    public void testAnnotatedMethodBelowMax_run() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "max30Method", false);
    }

    // Test combinations of {@link MinAndroidSdkLevel} and {@link MaxAndroidSdkLevel}

    @Test
    public void testAnnotatedMethodAboveMinBelowMax_run() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min28max30Method", false);
    }

    @Test
    public void testAnnotatedMethodBelowMinAboveMax_skip() throws Exception {
        expectShouldSkip(UnannotatedBaseClass.class, "min30max28Method", true);
    }
}
