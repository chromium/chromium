// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.annotation.CallSuper;

import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.test.OnlyRunIn.ProcessMode;
import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.policy.test.annotations.Policies;

import java.util.ArrayList;
import java.util.List;

/**
 * A custom runner for //android_webview instrumentation tests. Because WebView runs in
 * single-process mode on L-N and multi-process (AKA sandboxed renderer) mode on O+, this test
 * runner defaults to duplicating each test in both modes.
 *
 * <p>
 * Tests can instead be run in either single or multi-process only with {@link OnlyRunIn}. This can
 * be done if the test case needs this for correctness or to save infrastructure resources for tests
 * which exercise code which cannot depend on single vs. multi process.
 */
public final class AwJUnit4ClassRunner extends BaseJUnit4ClassRunner {
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
    protected List<PreTestHook> getPreTestHooks() {
        return addToList(super.getPreTestHooks(), Policies.getRegistrationHook());
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
        List<FrameworkMethod> result = new ArrayList<>();
        for (FrameworkMethod method : computeTestMethods()) {
            switch (processModeForMethod(method)) {
                case SINGLE_PROCESS:
                    result.add(method);
                    break;
                case MULTI_PROCESS:
                    result.add(new WebViewMultiProcessFrameworkMethod(method));
                    break;
                case SINGLE_AND_MULTI_PROCESS:
                default:
                    result.add(new WebViewMultiProcessFrameworkMethod(method));
                    result.add(method);
                    break;
            }
        }
        return result;
    }

    @Override
    protected void runChild(FrameworkMethod method, RunNotifier notifier) {
        CommandLineFlags.setUp(method.getMethod());
        if (method instanceof WebViewMultiProcessFrameworkMethod) {
            CommandLine.getInstance().appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
        }
        super.runChild(method, notifier);
    }

    /**
     * Custom FrameworkMethod class indicate this test method will run in multiprocess mode.
     *
     * The clas also add "__multiprocess_mode" postfix to the test name.
     */
    private static class WebViewMultiProcessFrameworkMethod extends FrameworkMethod {
        public WebViewMultiProcessFrameworkMethod(FrameworkMethod method) {
            super(method.getMethod());
        }

        @Override
        public String getName() {
            return super.getName() + "__multiprocess_mode";
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
