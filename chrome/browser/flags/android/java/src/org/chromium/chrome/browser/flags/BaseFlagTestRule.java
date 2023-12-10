// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertEquals;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.FeatureList;
import org.chromium.base.Flag;

import java.util.Map;

/** Test rule for testing subclasses of {@link Flag}. */
public class BaseFlagTestRule implements TestRule {
    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    tearDown();
                }
            }
        };
    }

    private void tearDown() {
        FeatureList.setTestFeatures(null);
        Flag.resetFlagsForTesting();
        CachedFlagUtils.resetFlagsForTesting();
    }

    static final String FEATURE_A = "FeatureA";
    static final String FEATURE_B = "FeatureB";

    static final Map<String, Boolean> A_OFF_B_ON = Map.of(FEATURE_A, false, FEATURE_B, true);
    static final Map<String, Boolean> A_OFF_B_OFF = Map.of(FEATURE_A, false, FEATURE_B, false);
    static final Map<String, Boolean> A_ON_B_OFF = Map.of(FEATURE_A, true, FEATURE_B, false);
    static final Map<String, Boolean> A_ON_B_ON = Map.of(FEATURE_A, true, FEATURE_B, true);

    static void assertIsEnabledMatches(Map<String, Boolean> state, Flag feature1, Flag feature2) {
        assertEquals(state.get(FEATURE_A), feature1.isEnabled());
        assertEquals(state.get(FEATURE_B), feature2.isEnabled());
    }
}
