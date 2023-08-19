// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.test.espresso.IdlingPolicies;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.concurrent.TimeUnit;

/**
 * Sets Espresso's master timeout policy. This helps reduce the time Espresso waits before failing
 * test cases that hang. This results in more useful stacks and errors messages than when the
 * process is killed from the outside.
 */
public final class EspressoIdleTimeoutRule implements TestRule {
    private final long mTimeout;
    private final TimeUnit mUnit;

    public EspressoIdleTimeoutRule(long timeout, TimeUnit unit) {
        mTimeout = timeout;
        mUnit = unit;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                IdlingPolicies.setMasterPolicyTimeout(mTimeout, mUnit);
                base.evaluate();
            }
        };
    }
}
