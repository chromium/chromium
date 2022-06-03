// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.BundleUtils;

/**
 * Ensures that BundleUtils#isBundle returns true for the duration of the test.
 */
public class BundleTestRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                boolean oldValue = BundleUtils.isBundle();
                try {
                    BundleUtils.setIsBundle(true);
                    base.evaluate();
                } finally {
                    BundleUtils.setIsBundle(oldValue);
                }
            }
        };
    }
}
