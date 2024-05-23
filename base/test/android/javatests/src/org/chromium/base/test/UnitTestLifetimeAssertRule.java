// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.RequiresRestart;

/** TestRule used to ensure we don't leak LifetimeAsserts in unit tests. */
public final class UnitTestLifetimeAssertRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                base.evaluate();
                Batch annotation = description.getTestClass().getAnnotation(Batch.class);
                if (annotation != null && annotation.value().equals(Batch.UNIT_TESTS)) {
                    if (description.getAnnotation(RequiresRestart.class) != null) return;
                    LifetimeAssert.assertAllInstancesDestroyedForTesting();
                }
            }
        };
    }
}
