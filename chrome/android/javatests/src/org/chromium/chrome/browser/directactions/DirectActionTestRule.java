// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.AppHooksImpl;
import org.chromium.chrome.test.ChromeActivityTestRule;

import java.util.function.Consumer;

/**
 * Enables direct actions in an activity started by a {@link ChromeActivityTestRule}.
 */
public class DirectActionTestRule implements TestRule {
    /**
     * A concrete implementation of DirectActionCoordinator, based on {@link
     * FakeDirectActionReporter}.
     */
    private DirectActionCoordinator mCoordinator;

    /** Returns the coordinator created by the rule. */
    @Nullable
    public DirectActionCoordinator getCoordinator() {
        return mCoordinator;
    }

    /** Disables direct action in the next activities that are started. */
    public void disableDirectActions() {
        // Since mCoordinator is null AppHooks will return null. Direct actions should not be
        // available.
        mCoordinator = null;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mCoordinator = new DirectActionCoordinator() {
                    @Override
                    protected DirectActionReporter createReporter(Consumer callback) {
                        return new FakeDirectActionReporter(callback);
                    }
                };
                AppHooks.setInstanceForTesting(new AppHooksImpl() {
                    @Override
                    public DirectActionCoordinator createDirectActionCoordinator() {
                        return mCoordinator;
                    }
                });
                try {
                    base.evaluate();
                } finally {
                    AppHooksImpl.setInstanceForTesting(null);
                }
            }
        };
    }
}
