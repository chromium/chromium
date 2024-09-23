// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.annotation.CallSuper;
import androidx.test.InstrumentationRegistry;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.test.OnlyRunIn.ProcessMode;
import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.policy.test.annotations.Policies;

import java.util.ArrayList;
import java.util.List;

/**
 * A custom runner for //android_webview instrumentation tests. Because WebView runs in
 * single-process mode on L-N and multi-process (AKA sandboxed renderer) mode on O+, this test
 * runner defaults to duplicating each test in both modes.
 *
 * <p>Tests can instead be run in single and/or multi process modes with {@link OnlyRunIn}:
 *
 * <ul>
 *   <li>SINGLE_PROCESS | MULTI_PROCESS: Run test only in single or multi process mode. This should
 *       only be used if the test case needs this for correctness.
 *   <li>EITHER_PROCESS: Run test in either single or multi process mode. This can be used to save
 *       infrastructure resources if the code being tested does not depend on the renderer process
 *       so the test will not benefit from duplication
 *   <li>SINGLE_AND_MULTI_PROCESS: Run test in both single and multi process modes.
 * </ul>
 */
public class AwJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    // This should match the definition in Android test runner scripts: bit.ly/3ynoREM
    private static final String MULTIPROCESS_TEST_NAME_SUFFIX = "__multiprocess_mode";

    // Accepted choices are "single" or "multiple", indicating that only tests
    // runnable in the specified process mode should run once in the specified mode.
    // The argument works at listing test stage in `List(...)`.
    private static final String PROCESS_MODE_FLAG = "AwJUnit4ClassRunner.ProcessMode";

    private final TestHook mWebViewMultiProcessHook =
            (targetContext, testMethod) -> {
                if (testMethod instanceof WebViewMultiProcessFrameworkMethod) {
                    CommandLine.getInstance().appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
                }
            };

    /**
     * Create an AwJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @param klass Test class to run
     * @throws InitializationError if the test class is malformed
     */
    public AwJUnit4ClassRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    @CallSuper
    @Override
    protected List<TestHook> getPreTestHooks() {
        return addToList(
                super.getPreTestHooks(), Policies.getRegistrationHook(), mWebViewMultiProcessHook);
    }

    private ProcessMode processModeForMethod(FrameworkMethod method) {
        // Prefer per-method annotations.
        //
        // Note: a per-class annotation might disagree with this. This can be confusing, but there's
        // no good option for us to forbid this (users won't see any exceptions we throw), and so we
        // instead enforce a priority.
        OnlyRunIn methodAnnotation = method.getAnnotation(OnlyRunIn.class);
        if (methodAnnotation != null) {
            return methodAnnotation.value();
        }
        // Per-class annotations have second priority.
        OnlyRunIn classAnnotation = method.getDeclaringClass().getAnnotation(OnlyRunIn.class);
        if (classAnnotation != null) {
            return classAnnotation.value();
        }
        // Default: run in both modes.
        return ProcessMode.SINGLE_AND_MULTI_PROCESS;
    }

    @Override
    protected List<FrameworkMethod> getChildren() {
        String processModeToExecute =
                InstrumentationRegistry.getArguments().getString(PROCESS_MODE_FLAG);
        boolean runSingleProcess =
                (processModeToExecute == null || "single".equals(processModeToExecute));
        boolean runMultiProcess =
                (processModeToExecute == null || "multiple".equals(processModeToExecute));
        if (!runSingleProcess && !runMultiProcess) {
            throw new IllegalArgumentException(
                    "'AwJUnit4ClassRunner.ProcessMode' value should be 'single' or 'multiple'.");
        }
        List<FrameworkMethod> singleProcessResults = new ArrayList<>();
        List<FrameworkMethod> multiProcessResults = new ArrayList<>();
        for (FrameworkMethod method : computeTestMethods()) {
            switch (processModeForMethod(method)) {
                case SINGLE_PROCESS:
                    singleProcessResults.add(method);
                    break;
                case MULTI_PROCESS:
                case EITHER_PROCESS:
                    multiProcessResults.add(new WebViewMultiProcessFrameworkMethod(method));
                    break;
                case SINGLE_AND_MULTI_PROCESS:
                default:
                    multiProcessResults.add(new WebViewMultiProcessFrameworkMethod(method));
                    singleProcessResults.add(method);
                    break;
            }
        }
        List<FrameworkMethod> result = new ArrayList<>();
        if (runSingleProcess) {
            result.addAll(singleProcessResults);
        }
        if (runMultiProcess) {
            result.addAll(multiProcessResults);
        }
        return result;
    }

    /**
     * Custom FrameworkMethod class indicate this test method will run in multiprocess mode.
     *
     * The class also adds MULTIPROCESS_TEST_NAME_SUFFIX postfix to the test name.
     */
    private static class WebViewMultiProcessFrameworkMethod extends FrameworkMethod {
        public WebViewMultiProcessFrameworkMethod(FrameworkMethod method) {
            super(method.getMethod());
        }

        @Override
        public String getName() {
            return super.getName() + MULTIPROCESS_TEST_NAME_SUFFIX;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof WebViewMultiProcessFrameworkMethod) {
                WebViewMultiProcessFrameworkMethod method =
                        (WebViewMultiProcessFrameworkMethod) obj;
                return super.equals(obj) && method.getName().equals(getName());
            }
            return false;
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + super.hashCode();
            result = 31 * result + getName().hashCode();
            return result;
        }
    }
}
