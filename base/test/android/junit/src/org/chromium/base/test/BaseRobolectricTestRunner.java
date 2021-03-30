// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runners.model.InitializationError;
import org.robolectric.DefaultTestLifecycle;
import org.robolectric.TestLifecycle;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.lang.reflect.Method;

/**
 * A Robolectric Test Runner that initializes base globals.
 */
public class BaseRobolectricTestRunner extends LocalRobolectricTestRunner {
    /**
     * Enables a per-test setUp / tearDown hook.
     */
    public static class BaseTestLifecycle extends DefaultTestLifecycle {
        @Override
        public void beforeTest(Method method) {
            ContextUtils.initApplicationContextForTests(
                    ApplicationProvider.getApplicationContext());
            ApplicationStatus.initialize(ApplicationProvider.getApplicationContext());
            UmaRecorderHolder.resetForTesting();
            CommandLine.init(null);
            super.beforeTest(method);
        }

        @Override
        public void afterTest(Method method) {
            try {
                LifetimeAssert.assertAllInstancesDestroyedForTesting();
            } finally {
                ApplicationStatus.destroyForJUnitTests();
                PathUtils.resetForTesting();
                super.afterTest(method);
            }
        }
    }

    public BaseRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    @Override
    protected Class<? extends TestLifecycle> getTestLifecycleClass() {
        return BaseTestLifecycle.class;
    }
}
