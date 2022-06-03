// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.InMemorySharedPreferencesContext;

/**
 * Holds setUp / tearDown logic common to all instrumentation tests.
 */
class BaseJUnit4TestRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                InMemorySharedPreferencesContext context =
                        BaseChromiumAndroidJUnitRunner.sInMemorySharedPreferencesContext;
                if (context == null) {
                    throw new IllegalStateException("BaseJUnit4TestRule requires that you use "
                            + "BaseChromiumAndroidJUnitRunner (or a subclass)");
                }
                base.evaluate();
            }
        };
    }

}
