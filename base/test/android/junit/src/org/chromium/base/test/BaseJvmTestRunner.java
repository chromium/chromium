// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.Ignore;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

import org.chromium.base.test.util.DisabledTest;

/** A custom {@link BlockJUnit4ClassRunner} that supports --run-disabled and @DisabledTest. */
public class BaseJvmTestRunner extends BlockJUnit4ClassRunner {
    public static final boolean sRunDisabled = System.getProperty("chromium.run_disabled") != null;

    public BaseJvmTestRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    public static boolean shouldRun(FrameworkMethod method) {
        if (sRunDisabled) {
            return true;
        }
        Class<?> clazz = method.getDeclaringClass();
        return method.getAnnotation(Ignore.class) == null
                && method.getAnnotation(DisabledTest.class) == null
                && clazz.getAnnotation(Ignore.class) == null
                && clazz.getAnnotation(DisabledTest.class) == null;
    }

    @Override
    protected boolean isIgnored(FrameworkMethod method) {
        return !shouldRun(method);
    }
}
