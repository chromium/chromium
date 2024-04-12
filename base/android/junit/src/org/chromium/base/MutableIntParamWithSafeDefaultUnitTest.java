// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_MAP;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link MutableIntegerParamWithSafeDefault}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MutableIntParamWithSafeDefaultUnitTest {
    private static final String PARAM_A = "a";
    private static final String PARAM_B = "b";

    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature() {
        makeParam(FEATURE_A, PARAM_A, 1);
        makeParam(FEATURE_A, PARAM_A, 2);
    }

    @Test
    public void testNativeNotInitialized() {
        MutableIntParamWithSafeDefault paramAA = makeParam(FEATURE_A, PARAM_A, 1);
        MutableIntParamWithSafeDefault paramAB = makeParam(FEATURE_A, PARAM_B, 2);

        assertEquals(1, paramAA.getValue());
        assertEquals(2, paramAB.getValue());
    }

    @Test
    public void testNativeInitialized() {
        MutableIntParamWithSafeDefault paramAA = makeParam(FEATURE_A, PARAM_A, 1);
        MutableIntParamWithSafeDefault paramAB = makeParam(FEATURE_A, PARAM_B, 2);
        MutableIntParamWithSafeDefault paramBA = makeParam(FEATURE_B, PARAM_A, 3);
        MutableIntParamWithSafeDefault paramBB = makeParam(FEATURE_B, PARAM_B, 4);

        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, true);
        testValues.addFieldTrialParamOverride(paramAA, "11");
        FeatureList.setTestValues(testValues);

        assertEquals(11, paramAA.getValue());
        assertEquals(2, paramAB.getValue());
        assertEquals(3, paramBA.getValue());
        assertEquals(4, paramBB.getValue());
    }

    private MutableIntParamWithSafeDefault makeParam(
            String featureName, String paramName, int defaultValue) {
        return new MutableIntParamWithSafeDefault(
                FEATURE_MAP, featureName, paramName, defaultValue);
    }
}
