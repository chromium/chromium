// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.junit.Assert.assertEquals;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.FeatureMap;
import org.chromium.base.FeatureParam;
import org.chromium.base.Flag;

import java.util.Map;

/** Test rule for testing subclasses of {@link Flag}. */
public class BaseFlagTestRule implements TestRule {
    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                Flag.useTemporaryFlagsCreatedForTesting();
                FeatureParam.useTemporaryParamsCreatedForTesting();
                base.evaluate();
            }
        };
    }

    public static final String FEATURE_A = "FeatureA";
    public static final String FEATURE_B = "FeatureB";

    public static final Map<String, Boolean> A_OFF_B_ON =
        Map.of(FEATURE_A, false, FEATURE_B, true);
    public static final Map<String, Boolean> A_OFF_B_OFF =
        Map.of(FEATURE_A, false, FEATURE_B, false);
    public static final Map<String, Boolean> A_ON_B_OFF =
        Map.of(FEATURE_A, true, FEATURE_B, false);
    public static final Map<String, Boolean> A_ON_B_ON =
        Map.of(FEATURE_A, true, FEATURE_B, true);

    /** A stub FeatureMap instance to create flags on. */
    public static final FeatureMap FEATURE_MAP =
            new FeatureMap() {
                @Override
                protected long getNativeMap() {
                    throw new UnsupportedOperationException(
                            "FeatureMap stub for testing does not support getting the flag value"
                                    + " across the native boundary, provide test override values"
                                    + " instead.");
                }
            };

    public static void assertIsEnabledMatches(
            Map<String, Boolean> state, Flag feature1, Flag feature2) {
        assertEquals(state.get(FEATURE_A), feature1.isEnabled());
        assertEquals(state.get(FEATURE_B), feature2.isEnabled());
    }
}
