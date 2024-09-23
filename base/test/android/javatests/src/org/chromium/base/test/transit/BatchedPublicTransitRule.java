// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.CallSuper;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Test rule for batched Public Transit tests.
 *
 * <p>Batched PublicTransit tests need to start and end in the same type of {@link Station}, which
 * is called the home station.
 *
 * @param <T> The Class of the home {@link Station}
 */
public class BatchedPublicTransitRule<T extends Station> implements TestRule {
    private final Class<T> mHomeStationType;
    private final boolean mExpectResetByTest;

    /**
     * @param homeStationType Class of the home station
     * @param expectResetByTest Whether the tests are responsible for resetting state before they
     *     finish. If false, state should be reset at the start of each test.
     */
    public BatchedPublicTransitRule(Class<T> homeStationType, boolean expectResetByTest) {
        mHomeStationType = homeStationType;
        mExpectResetByTest = expectResetByTest;
    }

    @Override
    @CallSuper
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                String testName = null;
                try {
                    testName = desc.getMethodName();
                    TrafficControl.onTestStarted(testName);
                    TransitAsserts.assertCurrentStationType(
                            mHomeStationType, "beginning of test", true);
                    base.evaluate();
                    if (mExpectResetByTest) {
                        TransitAsserts.assertCurrentStationType(
                                mHomeStationType, "end of test", false);
                    }
                } finally {
                    if (testName != null) {
                        TrafficControl.onTestFinished(testName);
                    }
                }
            }
        };
    }

    /**
     * Get the current station considering it the home station.
     *
     * @return the home station
     * @throws AssertionError if the current station is not of the expected home station type
     */
    public T getHomeStation() {
        TransitAsserts.assertCurrentStationType(
                mHomeStationType, "getting base station", /* allowNull= */ true);
        return (T) TrafficControl.getActiveStation();
    }
}
