// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.annotations.CheckDiscard;

/**
 * Attempts to provide additional information for Mockito errors that are hard to diagnose,
 * b/147584922 in particular.
 */
class MockitoErrorHandler implements TestRule {
    private static final String TAG = "MockitoErrorHandler";

    private static final String MOCKITO_ERROR =
            "Note: Proguard optimization is enabled and may cause exceptions when Mocking Derived "
            + "classes, or classes that implement interfaces whose methods are not kept. You may "
            + "need to add org.chromium.base.annotations.MockedInTests to such classes.";

    @CheckDiscard("")
    private void removedMethodUnderRelease() {}

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } catch (Throwable e) {
                    if ((e instanceof AbstractMethodError)
                            // UnfinishedStubbingException isn't on our build path.
                            || e.getClass().getSimpleName().equals("UnfinishedStubbingException")) {
                        try {
                            // Detect if code is being optimized.
                            getClass().getDeclaredMethod("removedMethodUnderRelease");
                        } catch (NoSuchMethodException ignored) {
                            Log.e(TAG, MOCKITO_ERROR);
                        }
                    }
                    throw e;
                }
            }
        };
    }
}
