// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ResettersForTesting;

/**
 * Ensures that all resetters are cleaned up after a test. The resetters are registered through
 * {@link ResettersForTesting#register(Runnable)} and are typically used whenever we have code that
 * has <code>public static void setFooForTesting(...)</code> constructs.
 */
class ResettersForTestingTestRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    ResettersForTesting.executeResetters();
                }
            }
        };
    }
}
