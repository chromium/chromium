// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for {@link FeatureList}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FeatureListUnitTest {

    private static final String FEATURE_A = "FeatureA";
    private static final String FEATURE_A_PARAM_1 = "Param1InFeatureA";
    private static final String FEATURE_A_PARAM_2 = "Param2InFeatureA";
    private static final String FEATURE_B = "FeatureB";

    @Test
    public void test_getTestValueForFeature_noOverride_throwsException() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> FeatureList.getTestValueForFeature(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFeature_canUseDefaults_noException() {
        FeatureList.setDisableNativeForTesting(false);
        Assert.assertNull(FeatureList.getTestValueForFeature(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFieldTrialParam_noOverride_returnsNull() {
        Assert.assertNull(FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
    }

    @Test
    public void test_getTestValueForFeature_override() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, true);
        FeatureList.setTestValues(testValues);

        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_A));

        testValues.addFeatureFlagOverride(FEATURE_A, false);
        FeatureList.setTestValues(testValues);

        Assert.assertEquals(false, FeatureList.getTestValueForFeature(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFieldTrialParam_override() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "paramValue");
        FeatureList.setTestValues(testValues);

        Assert.assertEquals(
                "paramValue",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));

        // Other params should still return null
        Assert.assertNull(FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    public void test_getTestValueForFeature_overrideOther_throwsException() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, true);
        FeatureList.setTestValues(testValues);

        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> FeatureList.getTestValueForFeature(FEATURE_B));
    }

    @Test
    public void test_mergeTestValues_noConflict() {
        TestValues testValues1 = new TestValues();
        testValues1.addFeatureFlagOverride(FEATURE_A, false);
        testValues1.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "paramValue1");

        TestValues testValues2 = new TestValues();
        testValues2.addFeatureFlagOverride(FEATURE_B, true);
        testValues2.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_2, "paramValue2");

        FeatureList.setTestValues(testValues1);
        FeatureList.mergeTestValues(testValues2, true);

        Assert.assertEquals(false, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_B));
        Assert.assertEquals(
                "paramValue1",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
        Assert.assertEquals(
                "paramValue2",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    public void test_mergeTestValues_replace() {
        TestValues testValues1 = new TestValues();
        testValues1.addFeatureFlagOverride(FEATURE_A, false);
        testValues1.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "false");

        TestValues testValues2 = new TestValues();
        testValues2.addFeatureFlagOverride(FEATURE_A, true);
        testValues2.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "true");

        FeatureList.setTestValues(testValues1);
        FeatureList.mergeTestValues(testValues2, true);

        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertEquals(
                "true", FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
    }

    @Test
    public void test_mergeTestValues_doNotReplace() {
        TestValues testValues1 = new TestValues();
        testValues1.addFeatureFlagOverride(FEATURE_A, false);
        testValues1.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "false");

        TestValues testValues2 = new TestValues();
        testValues2.addFeatureFlagOverride(FEATURE_A, true);
        testValues2.addFieldTrialParamOverride(FEATURE_A, FEATURE_A_PARAM_1, "true");

        FeatureList.setTestValues(testValues1);
        FeatureList.mergeTestValues(testValues2, false);

        Assert.assertEquals(false, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertEquals(
                "false", FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
    }

    @Test
    @EnableFeatures({FEATURE_A, FEATURE_B})
    public void test_setTestValues_replacesAnnotation() {
        FeatureList.setDisableNativeForTesting(false);

        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, false);
        FeatureList.setTestValues(testValues);

        Assert.assertEquals(false, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertNull(FeatureList.getTestValueForFeature(FEATURE_B));
    }

    @Test
    @EnableFeatures({FEATURE_A, FEATURE_B})
    public void test_mergeTestValues_replace_overridesAnnotation() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, false);
        FeatureList.mergeTestValues(testValues, true);

        Assert.assertEquals(false, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_B));
    }

    @Test
    @EnableFeatures({FEATURE_A, FEATURE_B})
    public void test_mergeTestValues_doNotReplace_doesNotOverrideAnnotation() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(FEATURE_A, false);
        FeatureList.mergeTestValues(testValues, false);

        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeature(FEATURE_B));
    }
}
