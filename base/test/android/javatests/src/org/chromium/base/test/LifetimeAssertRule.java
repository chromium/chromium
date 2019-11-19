// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.LifetimeAssert;

/**
 * Ensures that all object instances that use LifetimeAssert are destroyed.
 */
public class LifetimeAssertRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                base.evaluate();
                // Do not use try/finally so that lifetime asserts do not mask prior exceptions.
                LifetimeAssert.assertAllInstancesDestroyedForTesting();
            }
        };
    }
}
