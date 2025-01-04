// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for {@link FeatureOverrides}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FeatureOverridesUnitTest {

    private static final String FEATURE_A = "FeatureA";
    private static final String FEATURE_A_PARAM_1 = "Param1InFeatureA";
    private static final String FEATURE_A_PARAM_2 = "Param2InFeatureA";
    private static final String FEATURE_B = "FeatureB";

    @Test
    public void test_getTestValueForFeature_noOverride_throwsException() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> FeatureList.getTestValueForFeatureStrict(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFeature_canUseDefaults_noException() {
        FeatureList.setDisableNativeForTesting(false);
        Assert.assertNull(FeatureList.getTestValueForFeatureStrict(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFieldTrialParam_noOverride_returnsNull() {
        Assert.assertNull(FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
    }

    @Test
    public void test_getTestValueForFeature_override() {
        FeatureOverrides.enable(FEATURE_A);

        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_A));

        FeatureOverrides.disable(FEATURE_A);

        Assert.assertEquals(false, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
    }

    @Test
    public void test_getTestValueForFieldTrialParam_override() {
        FeatureOverrides.overrideParam(FEATURE_A, FEATURE_A_PARAM_1, "paramValue");

        Assert.assertEquals(
                "paramValue",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));

        // Other params should still return null
        Assert.assertNull(FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    public void test_getTestValueForFeature_overrideOther_throwsException() {
        FeatureOverrides.enable(FEATURE_A);

        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> FeatureList.getTestValueForFeatureStrict(FEATURE_B));
    }

    @Test
    public void test_apply_noConflict() {
        FeatureOverrides.newBuilder()
                .enable(FEATURE_A)
                .param(FEATURE_A_PARAM_1, "paramValue1")
                .apply();
        FeatureOverrides.newBuilder()
                .param(FEATURE_A, FEATURE_A_PARAM_2, "paramValue2")
                .disable(FEATURE_B)
                .apply();

        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
        Assert.assertEquals(false, FeatureList.getTestValueForFeatureStrict(FEATURE_B));
        Assert.assertEquals(
                "paramValue1",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
        Assert.assertEquals(
                "paramValue2",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    public void test_apply_replace() {
        FeatureOverrides.newBuilder()
                .enable(FEATURE_A)
                .param(FEATURE_A_PARAM_1, "paramValue1Original")
                .param(FEATURE_A_PARAM_2, "paramValue2")
                .enable(FEATURE_B)
                .apply();
        FeatureOverrides.newBuilder()
                .param(FEATURE_A, FEATURE_A_PARAM_1, "paramValue1Replaced")
                .disable(FEATURE_B)
                .apply();

        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
        Assert.assertEquals(false, FeatureList.getTestValueForFeatureStrict(FEATURE_B));
        Assert.assertEquals(
                "paramValue1Replaced",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
        Assert.assertEquals(
                "paramValue2",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    public void test_applyWithoutOverwrite_doNotReplace() {
        FeatureOverrides.newBuilder()
                .enable(FEATURE_A)
                .param(FEATURE_A_PARAM_1, "paramValue1Original")
                .param(FEATURE_A_PARAM_2, "paramValue2")
                .enable(FEATURE_B)
                .apply();
        FeatureOverrides.newBuilder()
                .param(FEATURE_A, FEATURE_A_PARAM_1, "paramValue1Replaced")
                .disable(FEATURE_B)
                .applyWithoutOverwrite();

        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_B));
        Assert.assertEquals(
                "paramValue1Original",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_1));
        Assert.assertEquals(
                "paramValue2",
                FeatureList.getTestValueForFieldTrialParam(FEATURE_A, FEATURE_A_PARAM_2));
    }

    @Test
    @EnableFeatures({FEATURE_A, FEATURE_B})
    public void test_apply_replacesAnnotation() {
        FeatureOverrides.newBuilder().disable(FEATURE_A).apply();

        Assert.assertEquals(false, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_B));
    }

    @Test
    @EnableFeatures({FEATURE_A, FEATURE_B})
    public void test_mergeTestValues_doNotReplace_doesNotOverrideAnnotation() {
        FeatureOverrides.newBuilder().disable(FEATURE_A).applyWithoutOverwrite();

        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_A));
        Assert.assertEquals(true, FeatureList.getTestValueForFeatureStrict(FEATURE_B));
    }
}
