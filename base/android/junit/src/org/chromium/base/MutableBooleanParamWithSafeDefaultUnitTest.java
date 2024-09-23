// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_MAP;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link MutableBooleanParamWithSafeDefault}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MutableBooleanParamWithSafeDefaultUnitTest {
    private static final String PARAM_A = "a";
    private static final String PARAM_B = "b";

    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature() {
        makeParam(FEATURE_A, PARAM_A, false);
        makeParam(FEATURE_A, PARAM_A, false);
    }

    @Test
    public void testNativeNotInitialized() {
        MutableBooleanParamWithSafeDefault paramAA = makeParam(FEATURE_A, PARAM_A, true);
        MutableBooleanParamWithSafeDefault paramAB = makeParam(FEATURE_A, PARAM_B, false);

        assertTrue(paramAA.getValue());
        assertFalse(paramAB.getValue());
    }

    @Test
    public void testNativeInitialized() {
        MutableBooleanParamWithSafeDefault paramAA = makeParam(FEATURE_A, PARAM_A, false);
        MutableBooleanParamWithSafeDefault paramAB = makeParam(FEATURE_A, PARAM_B, false);
        MutableBooleanParamWithSafeDefault paramBA = makeParam(FEATURE_B, PARAM_A, false);
        MutableBooleanParamWithSafeDefault paramBB = makeParam(FEATURE_B, PARAM_B, false);

        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, true);
        testValues.addFieldTrialParamOverride(paramAA, "true");
        FeatureList.setTestValues(testValues);

        assertTrue(paramAA.getValue());
        assertFalse(paramAB.getValue());
        assertFalse(paramBA.getValue());
        assertFalse(paramBB.getValue());
    }

    private MutableBooleanParamWithSafeDefault makeParam(
            String featureName, String paramName, boolean defaultValue) {
        return new MutableBooleanParamWithSafeDefault(
                FEATURE_MAP, featureName, paramName, defaultValue);
    }
}
