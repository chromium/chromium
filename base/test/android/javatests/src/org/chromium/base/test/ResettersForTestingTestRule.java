// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ResettersForTesting;

/**
 * Include this rule when using ParameterizedRobolectricTest.
 * Resetters registered during @BeforeClass will be reset after each method.
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
                    // We cannot guarantee that this Rule will be evaluated first, so never
                    // call setMethodMode(), and reset class resetters after each method.
                    ResettersForTesting.onAfterClass();
                }
            }
        };
    }
}
