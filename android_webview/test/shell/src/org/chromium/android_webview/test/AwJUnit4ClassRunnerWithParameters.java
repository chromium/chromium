// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.runner.Runner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.parameterized.ParametersRunnerFactory;
import org.junit.runners.parameterized.TestWithParameters;

import java.util.ArrayList;
import java.util.List;

/**
 * A parameterization-aware version of the custom runner
 * for //android_webview instrumentation tests.
 */
public class AwJUnit4ClassRunnerWithParameters extends AwJUnit4ClassRunner {
    private final Object[] mParameters;
    private final String mName;

    /**
     * Create an AwJUnit4ClassRunnerWithParameters to run {@code test} and initialize values
     *
     * @param test Wrapper containing information on the Test class to run and parameters
     * @throws InitializationError if the test class is malformed
     */
    public AwJUnit4ClassRunnerWithParameters(TestWithParameters test) throws InitializationError {
        super(test.getTestClass().getJavaClass());
        this.mParameters = test.getParameters().toArray(new Object[test.getParameters().size()]);

        // Appease logdog stream name validation
        this.mName = test.getName().replace("[", "__").replace(']', '_');
    }

    @Override
    protected List<FrameworkMethod> getChildren() {
        List<FrameworkMethod> result = new ArrayList<>();
        for (FrameworkMethod method : super.getChildren()) {
            if (method.getAnnotation(SkipMutations.class) == null
                    || ((AwSettingsMutation) mParameters[0]).getMutation() == null) {
                result.add(method);
            }
        }
        return result;
    }

    @Override
    public Object createTest() throws Exception {
        return getTestClass().getOnlyConstructor().newInstance(mParameters);
    }

    @Override
    protected void validateConstructor(List<Throwable> errors) {
        validateOnlyOneConstructor(errors);
        // skip zero-arg validation
    }

    @Override
    protected String testName(FrameworkMethod method) {
        return method.getName() + mName;
    }

    public static class Factory implements ParametersRunnerFactory {
        @Override
        public Runner createRunnerForTestWithParameters(TestWithParameters test)
                throws InitializationError {
            return new AwJUnit4ClassRunnerWithParameters(test);
        }
    }
}
